
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_stats_data_source.h"
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_counters.h"

#include <optional>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

#include "protos/perfetto/common/arm_gpu_counters.pbzero.h"
#include "protos/perfetto/config/profiling/arm_gpu_stats_config.pbzero.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

using protos::pbzero::ArmGpuStatsConfig;

namespace profiling {

namespace {
constexpr size_t kReadBufSize = 1024 * 16;

base::ScopedFile OpenReadOnly(const char* path) {
  base::ScopedFile fd(base::OpenFile(path, O_RDONLY));
  if (!fd)
    PERFETTO_PLOG("Failed opening %s", path);
  return fd;
}

std::string get_sample_value(const hwcpipe::counter_sample& sample) {
  switch (sample.type) {
    case hwcpipe::counter_sample::type::uint64:
      return std::to_string(sample.value.uint64);
    case hwcpipe::counter_sample::type::float64:
      return std::to_string(sample.value.float64);
    default:
      return "unknown";
  }
}

uint32_t ClampTo10Ms(uint32_t period_ms, const char* counter_name) {
  if (period_ms > 0 && period_ms < 10) {
    PERFETTO_ILOG("%s %" PRIu32
                  " is less than minimum of 10ms. Increasing to 10ms.",
                  counter_name, period_ms);
    return 10;
  }
  return period_ms;
}

}  // namespace

// static
const DimprofdDataSource::Descriptor ArmGpuStatsDataSource::descriptor = {
    /* name */ "linux.arm_gpu_stats",
    /* flags */ Descriptor::kFlagsNone,
    /* fill_descriptor_func */ nullptr,
};

ArmGpuStatsDataSource::~ArmGpuStatsDataSource() {
   if (!gpuinfo_counters_.empty()) {
    std::error_code ec = arm_sampler_->stop_sampling();
    if (ec) {
      PERFETTO_ELOG("stop_sampling failed by %s", ec.message().c_str());
    }
  }
}

ArmGpuStatsDataSource::ArmGpuStatsDataSource(
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer,
    const DataSourceConfig& ds_config)
    : DimprofdDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  using protos::pbzero::ArmGpuStatsConfig;
  ArmGpuStatsConfig::Decoder cfg(ds_config.arm_gpu_stats_config_raw());

  read_buf_ = base::PagedMemory::Allocate(kReadBufSize);

  uint32_t period_ms = ClampTo10Ms(cfg.gpuinfo_period_ms(), "gpu_period_ms");
  tick_period_ms_ = period_ms;

  // setup hwcpipe
  auto gpu = hwcpipe::gpu(0);
  if (!gpu) {
    PERFETTO_ELOG("Mali GPU device 0 is missing");
    return;
  }

  arm_sampler_config_ = std::make_unique<hwcpipe::sampler_config>(gpu);
  std::error_code ec;

  constexpr size_t kMaxArmGpuInfoEnum = protos::pbzero::ArmGpuCounters_MAX;
  std::bitset<kMaxArmGpuInfoEnum + 1> gpuinfo_counters_enabled{};
  if (!cfg.has_arm_gpu_counters())
    gpuinfo_counters_enabled.set();
  for (auto it = cfg.arm_gpu_counters(); it; ++it) {
    uint32_t counter = static_cast<uint32_t>(*it);
    if (counter > 0 && counter <= kMaxArmGpuInfoEnum) {
      gpuinfo_counters_enabled.set(counter);
    } else {
      PERFETTO_DFATAL("ArmGpu counter out of bounds %u", counter);
    }
  }
  for (size_t i = 0; i < base::ArraySize(kArmGpuInfoPBKeys); i++) {
    const auto& k = kArmGpuInfoPBKeys[i];
    if (gpuinfo_counters_enabled[static_cast<size_t>(k.id)] == false) {
      continue;
    }
    if (!k.type.has_value()) {
      continue;
    }
    ec = arm_sampler_config_->add_counter(
        k.type.value());  // FIXME: 如果失败，是否污染sampler的counter_
    if (ec) {
      PERFETTO_ELOG("%s counter not supported by this GPU.", k.str);
      continue;
    }
    gpuinfo_counters_.emplace(k.str, k);
  }
  if (gpuinfo_counters_.empty()) {
    PERFETTO_ELOG("counter is empty");
    return;
  }

  arm_sampler_ =
      std::make_unique<hwcpipe::sampler<>>(arm_sampler_config_.get());
}

void ArmGpuStatsDataSource::Start() {
  auto weak_this = GetWeakPtr();
  if (gpuinfo_counters_.empty()) {
    PERFETTO_ELOG("GPU Sampler no Counters");
    return;
  }
  std::error_code ec = arm_sampler_->start_sampling();
  if (ec) {
    PERFETTO_ELOG("GPU Sampler start_sampling failed by %s.",
                  ec.message().c_str());
    return;
  }

  task_runner_->PostTask(std::bind(&ArmGpuStatsDataSource::Tick, weak_this));
}

// static
void ArmGpuStatsDataSource::Tick(
    base::WeakPtr<ArmGpuStatsDataSource> weak_this) {
  if (!weak_this)
    return;
  ArmGpuStatsDataSource& thiz = *weak_this;

  uint32_t period_ms = thiz.tick_period_ms_;
  uint32_t delay_ms =
      period_ms -
      static_cast<uint32_t>(base::GetWallTimeMs().count() % period_ms);
  thiz.task_runner_->PostDelayedTask(
      std::bind(&ArmGpuStatsDataSource::Tick, weak_this), delay_ms);
  thiz.ReadArmGpuStats();
}

void ArmGpuStatsDataSource::ReadArmGpuStats() {
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));

  protos::pbzero::ArmGpuStats* arm_gpu_stats = packet->set_arm_gpu_stats();

  ReadGpuSampler(arm_gpu_stats);
  ReadGpuFreq(arm_gpu_stats);
  ReadGpuFreqV2(arm_gpu_stats);

  arm_gpu_stats->set_collection_end_timestamp(
      static_cast<uint64_t>(base::GetBootTimeNs().count()));
}

base::ScopedDir ArmGpuStatsDataSource::OpenDirAndLogOnErrorOnce(
    const std::string& dir_path,
    bool* already_logged) {
  base::ScopedDir dir(opendir(dir_path.c_str()));
  if (!dir && !(*already_logged)) {
    PERFETTO_PLOG("Failed to open %s", dir_path.c_str());
    *already_logged = true;
  }
  return dir;
}

void ArmGpuStatsDataSource::ReadGpuSampler(
    protos::pbzero::ArmGpuStats* arm_gpu_stats) {
  if (gpuinfo_counters_.size() > 0) {
    hwcpipe::counter_sample sample;
    std::error_code ec;

    ec = arm_sampler_->sample_now();
    if (ec) {
      PERFETTO_ELOG("sample_now failed by %s", ec.message().c_str());
      return;
    }

    for (const auto& pair : gpuinfo_counters_) {
      // std::cout << pair.first << ": " << pair.second << std::endl;
      auto cur_counter = pair.second;

      ec = arm_sampler_->get_counter_value(cur_counter.type.value(), sample);
      if (ec) {
        PERFETTO_ELOG("sample %s failed by %s", cur_counter.str,
                      ec.message().c_str());
      } else {
        PERFETTO_DLOG("print_sample_value: %d %s %s", cur_counter.id, cur_counter.str,
                      get_sample_value(sample).c_str());
        auto* arm_gpuinfo = arm_gpu_stats->add_arm_gpuinfo();
        arm_gpuinfo->set_key(
            static_cast<protos::pbzero::ArmGpuCounters>(cur_counter.id));
        
        switch (sample.type) {
          case hwcpipe::counter_sample::type::uint64: {
            PERFETTO_DLOG("cur int value %ld", sample.value.uint64);
            arm_gpuinfo->set_val_type(0);
            arm_gpuinfo->set_int_value(sample.value.uint64);
            break;
          }
          case hwcpipe::counter_sample::type::float64: {
            PERFETTO_DLOG("cur double value %f", sample.value.float64);
            arm_gpuinfo->set_val_type(1);
            arm_gpuinfo->set_double_value(sample.value.float64);
            break;
          }
          default:
            arm_gpuinfo->set_int_value(0);  // TODO: default value
        }
      }
    }
  }
}

void ArmGpuStatsDataSource::ReadGpuFreq(
    protos::pbzero::ArmGpuStats* arm_gpu_stats) {
  std::string base_dir = "/proc/gpufreq/";
  base::ScopedDir gpufreq_dir =
      OpenDirAndLogOnErrorOnce(base_dir, &gpufreq_error_logged_);
  if (!gpufreq_dir) {
    return;
  }

  const char* gpufreq_base_path = "/proc/gpufreq";
  const char* freq_file_name = "gpufreq_var_dump";
  base::StackString<256> gpu_freq_path("%s/%s", gpufreq_base_path,
                                       freq_file_name);

  base::ScopedFile fd = OpenReadOnly(gpu_freq_path.c_str());
  if (!fd && !gpufreq_error_logged_) {
    gpufreq_error_logged_ = true;
    PERFETTO_PLOG("Failed to open %s", gpu_freq_path.c_str());
    arm_gpu_stats->add_gpufreq_hz(0);
    return;
  }

  size_t rsize = ReadFile(&fd, gpu_freq_path.c_str());
  if (!rsize) {
    arm_gpu_stats->add_gpufreq_hz(0);
    return;
  }

  // TODO: no devices to get the content of /proc/gpufreq/gpufreq_var_dump
  // Just assume the content only have the value of freq.
  const char* file_content = static_cast<char*>(read_buf_.Get());
  auto value = static_cast<uint64_t>(strtoll(file_content, nullptr, 10));
  arm_gpu_stats->add_gpufreq_hz(value);
}

void ArmGpuStatsDataSource::ReadGpuFreqV2(
    protos::pbzero::ArmGpuStats* arm_gpu_stats) {
  std::string base_dir = "/proc/gpufreqv2/";
  base::ScopedDir gpufreqv2_dir =
      OpenDirAndLogOnErrorOnce(base_dir, &gpufreqv2_error_logged_);
  if (!gpufreqv2_dir) {
    return;
  }

  const char* gpufreq_base_path = "/proc/gpufreqv2";
  const char* freq_file_name = "gpufreq_status";
  base::StackString<256> gpu_freq_path("%s/%s", gpufreq_base_path,
                                       freq_file_name);

  base::ScopedFile fd = OpenReadOnly(gpu_freq_path.c_str());
  if (!fd && !gpufreq_error_logged_) {
    gpufreq_error_logged_ = true;
    PERFETTO_PLOG("Failed to open %s", gpu_freq_path.c_str());
    arm_gpu_stats->add_gpufreq_v2_hz(0);
    return;
  }

  size_t rsize = ReadFile(&fd, gpu_freq_path.c_str());
  if (!rsize) {
    arm_gpu_stats->add_gpufreq_v2_hz(0);
    return;
  }

  // grep line like
  // [STACK OPP]      Index: 51, Freq:   26000, Volt:  49000, Vsram:  75000
  char* buf = static_cast<char*>(read_buf_.Get());
  for (base::StringSplitter lines(buf, rsize, '\n'); lines.Next();) {
    base::StringView line(lines.cur_token(), lines.cur_token_size());
    if (line.StartsWith("[STACK OPP]")) {
      for (base::StringSplitter words(line.ToStdString(), ' '); words.Next();) {
        if (!base::StartsWith(words.cur_token(), "Freq"))
          continue;

        words.Next();
        // PERFETTO_DLOG("the token after Freq: %s", words.cur_token());  // 26000,
        // Strip suffix ",".
        std::string maybe_freq = std::string(words.cur_token())
                                     .substr(0, words.cur_token_size() - 1);
        auto value = static_cast<uint64_t>(strtoll(maybe_freq.c_str(), nullptr, 10));
        arm_gpu_stats->add_gpufreq_v2_hz(value);
        return;
      }
    }
  }
  // extract failed
  PERFETTO_PLOG("Failed to extract %s", gpu_freq_path.c_str());
  arm_gpu_stats->add_gpufreq_v2_hz(0);
}

base::WeakPtr<ArmGpuStatsDataSource> ArmGpuStatsDataSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void ArmGpuStatsDataSource::Flush(FlushRequestID,
                                  std::function<void()> callback) {
  PERFETTO_DLOG("doFlush");

  // if (!gpuinfo_counters_.empty()) {
  //   std::error_code ec = arm_sampler_->stop_sampling();
  //   if (ec) {
  //     PERFETTO_ELOG("stop_sampling failed by %s", ec.message().c_str());
  //   }
  // }
  writer_->Flush(callback);
}

size_t ArmGpuStatsDataSource::ReadFile(base::ScopedFile* fd, const char* path) {
  if (!*fd)
    return 0;
  ssize_t res = pread(**fd, read_buf_.Get(), kReadBufSize - 1, 0);
  if (res <= 0) {
    PERFETTO_PLOG("Failed reading %s", path);
    fd->reset();
    return 0;
  }
  size_t rsize = static_cast<size_t>(res);
  static_cast<char*>(read_buf_.Get())[rsize] = '\0';
  return rsize + 1;  // Include null terminator in the count.
}

}  // namespace profiling
}  // namespace perfetto


#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_stats_data_source.h"
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_counters.h"

#include <optional>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
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

// static std::map<std::string, hwcpipe_counter> create_counter_map() {
//   std::map<std::string, hwcpipe_counter> counter_map;
//   for (const auto& counter : kArmGpuInfoHwcpipeCounters) {
//     counter_map[counter.str] = counter.item;
//   }
//   return counter_map;
// }

// hwcpipe_counter find_counter_map(const char* key) {
//   static const auto counter_map = create_counter_map();
//   auto it = counter_map.find(key);
//   if (it != counter_map.end()) {
//     return it->second;
//   }
//   throw std::runtime_error("Counter not found");
// }

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
  // arm_sampler_ = nullptr;
  // arm_sampler_config_ = nullptr;
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
  thiz.ReadGpuSampler();
}

void ArmGpuStatsDataSource::ReadGpuSampler() {
  // PERFETTO_METATRACE_SCOPED(TAG_PROC_POLLERS, READ_SYS_STATS); // need
  // metatrace
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));

  protos::pbzero::ArmGpuStats* arm_gpu_stats = packet->set_arm_gpu_stats();

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
        PERFETTO_LOG("print_sample_value %s %s", cur_counter.str,
                     get_sample_value(sample).c_str());
        PERFETTO_LOG("print_sample_value %s %s", cur_counter.str,
                     get_sample_value(sample).c_str());
        auto* arm_gpuinfo = arm_gpu_stats->add_arm_gpuinfo();
        arm_gpuinfo->set_key(
            static_cast<protos::pbzero::ArmGpuCounters>(cur_counter.id));
        switch (sample.type) {
          case hwcpipe::counter_sample::type::uint64: {
            arm_gpuinfo->set_int_value(sample.value.uint64);
            break;
          }
          case hwcpipe::counter_sample::type::float64: {
            arm_gpuinfo->set_double_value(sample.value.float64);
            break;
          }
          default:
            arm_gpuinfo->set_int_value(0);  // TODO:
        }
      }
    }

    // ec = arm_sampler_->get_counter_value(MaliGPUActiveCy, sample);
    // if (ec) {
    //   PERFETTO_ELOG("sample MaliGPUActiveCy failed by %s",
    //                 ec.message().c_str());
    // } else {
    //   PERFETTO_LOG("print_sample_value MaliGPUActiveCy %s",
    //                get_sample_value(sample).c_str());
    // }

    ReadGpuCounters(/*gpu_counters*/);
  }
}

void ArmGpuStatsDataSource::ReadGpuCounters() {
  PERFETTO_LOG("ReadGpuCounters running");
}

base::WeakPtr<ArmGpuStatsDataSource> ArmGpuStatsDataSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void ArmGpuStatsDataSource::Flush(FlushRequestID,
                                  std::function<void()> callback) {
  PERFETTO_LOG("doFlush");

  if (!gpuinfo_counters_.empty()) {
    std::error_code ec = arm_sampler_->stop_sampling();
    if (ec) {
      PERFETTO_ELOG("stop_sampling failed by %s", ec.message().c_str());
    }
  }
  writer_->Flush(callback);
}

std::string ArmGpuStatsDataSource::ReadFile(std::string path) {
  std::string contents;
  if (!base::ReadFile(path, &contents))
    return "";
  return contents;
}

}  // namespace profiling
}  // namespace perfetto


#include "src/profiling/mtk/arm_gpu_counter/arm_gpu_counter_data_source.h"

#include <optional>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

// #include "protos/perfetto/trace/system_info/cpu_info.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace profiling {

namespace {

// const char* get_product_family_name(hwcpipe::device::gpu_family f) {
//   using gpu_family = hwcpipe::device::gpu_family;

//   switch (f) {
//     case gpu_family::bifrost:
//       return "Bifrost";
//     case gpu_family::midgard:
//       return "Midgard";
//     case gpu_family::valhall:
//       return "Valhall";
//     case gpu_family::fifthgen:
//       return "Arm 5th Gen";
//     default:
//       return "Unknown";
//   }
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

}  // namespace

// static
const DimprofdDataSource::Descriptor ArmGpuCounterDataSource::descriptor = {
    /* name */ "linux.arm_gpu_counter",
    /* flags */ Descriptor::kFlagsNone,
    /* fill_descriptor_func */ nullptr,
};

ArmGpuCounterDataSource::~ArmGpuCounterDataSource() {
  // arm_sampler_ = nullptr;
  // arm_sampler_config_ = nullptr;
}

ArmGpuCounterDataSource::ArmGpuCounterDataSource(
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer)
    : DimprofdDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  tick_period_ms_ = 500;  // TODO: get by config

  auto gpu = hwcpipe::gpu(0);
  if (!gpu) {
    PERFETTO_ELOG("Mali GPU device 0 is missing");
    return;
  }

  arm_sampler_config_ = std::make_unique<hwcpipe::sampler_config>(gpu);
  std::error_code ec;

  ec = arm_sampler_config_->add_counter(MaliGPUActiveCy);
  if (ec) {
    PERFETTO_ELOG("GPU Active Cycles counter not supported by this GPU.");
    return;
  } else {
    arm_counter_size_ += 1;
  }

  arm_sampler_ =
      std::make_unique<hwcpipe::sampler<>>(arm_sampler_config_.get());
}

void ArmGpuCounterDataSource::Start() {
  auto weak_this = GetWeakPtr();
  std::error_code ec = arm_sampler_->start_sampling();
  if (ec) {
    PERFETTO_ELOG("GPU Sampler start_sampling failed by %s.", ec.message().c_str());
    return;
  }

  task_runner_->PostTask(std::bind(&ArmGpuCounterDataSource::Tick, weak_this));
}

// static
void ArmGpuCounterDataSource::Tick(
    base::WeakPtr<ArmGpuCounterDataSource> weak_this) {
  if (!weak_this)
    return;
  ArmGpuCounterDataSource& thiz = *weak_this;

  uint32_t period_ms = thiz.tick_period_ms_;
  uint32_t delay_ms =
      period_ms -
      static_cast<uint32_t>(base::GetWallTimeMs().count() % period_ms);
  thiz.task_runner_->PostDelayedTask(
      std::bind(&ArmGpuCounterDataSource::Tick, weak_this), delay_ms);
  thiz.ReadGpuSampler();
}

void ArmGpuCounterDataSource::ReadGpuSampler() {
  // PERFETTO_METATRACE_SCOPED(TAG_PROC_POLLERS, READ_SYS_STATS); // need
  // metatrace
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));

  if (arm_counter_size_ > 0) {
    hwcpipe::counter_sample sample;
    std::error_code ec;

    ec = arm_sampler_->sample_now();
    if (ec) {
      PERFETTO_ELOG("sample_now failed by %s", ec.message().c_str());
      return;
    }

    ec = arm_sampler_->get_counter_value(MaliGPUActiveCy, sample);
    if (ec) {
      PERFETTO_ELOG("sample MaliGPUActiveCy failed by %s", ec.message().c_str());
    } else {
      PERFETTO_LOG("print_sample_value MaliGPUActiveCy %s",
                    get_sample_value(sample).c_str());
    }

    ReadGpuCounters(/*gpu_counters*/);
  }

  PERFETTO_LOG("ReadGpuSampler running");
}

void ArmGpuCounterDataSource::ReadGpuCounters() {}

base::WeakPtr<ArmGpuCounterDataSource> ArmGpuCounterDataSource::GetWeakPtr()
    const {
  return weak_factory_.GetWeakPtr();
}

void ArmGpuCounterDataSource::Flush(FlushRequestID,
                                    std::function<void()> callback) {
  PERFETTO_LOG("doFlush");
  std::error_code ec = arm_sampler_->stop_sampling();
  if (ec) {
    PERFETTO_ELOG("stop_sampling failed by %s", ec.message().c_str());
  }
  writer_->Flush(callback);
}

std::string ArmGpuCounterDataSource::ReadFile(std::string path) {
  std::string contents;
  if (!base::ReadFile(path, &contents))
    return "";
  return contents;
}

}  // namespace profiling
}  // namespace perfetto

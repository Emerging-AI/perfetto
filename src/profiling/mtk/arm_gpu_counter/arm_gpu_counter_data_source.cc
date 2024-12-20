
#include "src/profiling/mtk/arm_gpu_counter/arm_gpu_counter_data_source.h"

#include <optional>

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

namespace {}  // namespace

// static
const DimprofdDataSource::Descriptor ArmGpuCounterDataSource::descriptor = {
    /* name */ "linux.arm_gpu_counter",
    /* flags */ Descriptor::kFlagsNone,
    /* fill_descriptor_func */ nullptr,
};

ArmGpuCounterDataSource::~ArmGpuCounterDataSource() = default;

ArmGpuCounterDataSource::ArmGpuCounterDataSource(
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer)
    : DimprofdDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  tick_period_ms_ = 20;  // TODO: get by config
}

void ArmGpuCounterDataSource::Start() {
  auto weak_this = GetWeakPtr();
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
  thiz.ReadGpuCounter();
}

void ArmGpuCounterDataSource::ReadGpuCounter() {
  // PERFETTO_METATRACE_SCOPED(TAG_PROC_POLLERS, READ_SYS_STATS); // need
  // metatrace
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));

  PERFETTO_LOG("ReadGpuCounter running");
}


base::WeakPtr<ArmGpuCounterDataSource> ArmGpuCounterDataSource::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

void ArmGpuCounterDataSource::Flush(FlushRequestID,
                                    std::function<void()> callback) {
  PERFETTO_LOG("doFlush");
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

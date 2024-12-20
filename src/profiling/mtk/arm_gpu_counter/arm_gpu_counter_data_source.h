#ifndef SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_
#define SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_

#include <memory>

#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "src/profiling/mtk/dimprofd_data_source.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

namespace profiling {

class ArmGpuCounterDataSource : public DimprofdDataSource {
 public:
  static const DimprofdDataSource::Descriptor descriptor;

  ArmGpuCounterDataSource(base::TaskRunner*,
                          TracingSessionID,
                          std::unique_ptr<TraceWriter> writer);
  ~ArmGpuCounterDataSource() override;

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  base::WeakPtr<ArmGpuCounterDataSource> GetWeakPtr() const;

  // Virtual for testing.
  virtual std::string ReadFile(std::string path);

 private:
  static void Tick(base::WeakPtr<ArmGpuCounterDataSource>);

  void ReadGpuCounter();  // Virtual for testing.

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;

  uint32_t tick_period_ms_ = 0;

  base::WeakPtrFactory<ArmGpuCounterDataSource> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_

#ifndef SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_
#define SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_

#include <memory>
#include <map>
#include <string>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/profiling/mtk/dimprofd_data_source.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_counters.h"

#include <device/product_id.hpp>
#include <hwcpipe/counter_database.hpp>
#include <hwcpipe/gpu.hpp>
#include <hwcpipe/sampler.hpp>

#include <iomanip>

#include <unistd.h>

namespace perfetto {

namespace base {
class TaskRunner;
}

namespace profiling {

class ArmGpuStatsDataSource : public DimprofdDataSource {
 public:
  static const DimprofdDataSource::Descriptor descriptor;

  ArmGpuStatsDataSource(base::TaskRunner*,
                          TracingSessionID,
                          std::unique_ptr<TraceWriter> writer,
                          const DataSourceConfig&);
  ~ArmGpuStatsDataSource() override;

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  base::WeakPtr<ArmGpuStatsDataSource> GetWeakPtr() const;

  // Virtual for testing.
  virtual base::ScopedDir OpenDirAndLogOnErrorOnce(const std::string& dir_path,
                                                   bool* already_logged);

 protected:
  bool gpufreq_error_logged_ = false;
  bool gpufreqv2_error_logged_ = false;

 private:
  struct CStrCmp {
    bool operator()(const char* a, const char* b) const {
      return strcmp(a, b) < 0;
    }
  };

  static void Tick(base::WeakPtr<ArmGpuStatsDataSource>);

  void ReadArmGpuStats();  // Virtual for testing.
  void ReadGpuSampler(protos::pbzero::ArmGpuStats* arm_gpu_stats);
  void ReadGpuFreq(protos::pbzero::ArmGpuStats* arm_gpu_stats);
  void ReadGpuFreqV2(protos::pbzero::ArmGpuStats* arm_gpu_stats);

  size_t ReadFile(base::ScopedFile*, const char* path);

  std::unique_ptr<hwcpipe::sampler<>> arm_sampler_ = nullptr;
  std::unique_ptr<hwcpipe::sampler_config> arm_sampler_config_ = nullptr;
  uint32_t tick_period_ms_ = 0;

  base::PagedMemory read_buf_;
  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;
  std::map<const char*, KeyAndCounter, CStrCmp> gpuinfo_counters_;


  base::WeakPtrFactory<ArmGpuStatsDataSource> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MTK_DIMPROFD_ARM_GPU_COUNTER_ARM_GPU_COUNTER_DATA_SOURCE_H_

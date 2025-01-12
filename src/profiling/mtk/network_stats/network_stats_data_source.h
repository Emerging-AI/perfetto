#ifndef SRC_PROFILING_MTK_DIMPROFD_NETWORK_DATA_SOURCE_H_
#define SRC_PROFILING_MTK_DIMPROFD_NETWORK_DATA_SOURCE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/profiling/mtk/dimprofd_data_source.h"

#include <device/product_id.hpp>

#include <iomanip>
#include <unistd.h>

namespace perfetto {

namespace base {
class TaskRunner;
}

namespace profiling {

class NetworkStatsDataSource : public DimprofdDataSource {
 public:
  static const DimprofdDataSource::Descriptor descriptor;

  NetworkStatsDataSource(base::TaskRunner*,
                          TracingSessionID,
                          std::unique_ptr<TraceWriter> writer,
                          const DataSourceConfig& ds_config);
  ~NetworkStatsDataSource() override;

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  base::WeakPtr<NetworkStatsDataSource> GetWeakPtr() const;

  // Virtual for testing.
  virtual std::string ReadFile(const std::string& path);

 private:
  static void Tick(base::WeakPtr<NetworkStatsDataSource>);

  void ReadNetworkStats();   // Mainly Read Function
  void ReadNetworkInfo(const std::string& protocol, const pid_t pid, uint64_t& tb, uint64_t& rb);

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;

  std::vector<std::string> cmdlines_;
  std::set<pid_t> pids_;

  uint32_t tick_period_ms_ = 0;

  base::WeakPtrFactory<NetworkStatsDataSource> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MTK_DIMPROFD_NETWORK_DATA_SOURCE_H_

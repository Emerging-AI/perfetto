#ifndef SRC_PROFILING_MTK_DIMPROFD_PRODUCER_H_
#define SRC_PROFILING_MTK_DIMPROFD_PRODUCER_H_

#include <memory>
#include <unordered_map>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

#include "src/profiling/mtk/dimprofd_data_source.h"

namespace perfetto {
namespace profiling {

class DimprofdProducer : public Producer {
 public:
  DimprofdProducer();
  ~DimprofdProducer() override;

  static DimprofdProducer* GetInstance();

  // Producer Impl:
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(FlushRequestID,
             const DataSourceInstanceID* /*data_source_ids*/,
             size_t /*num_data_sources*/,
             FlushFlags) override;
  void ClearIncrementalState(const DataSourceInstanceID* /*data_source_ids*/,
                             size_t /*num_data_sources*/) override;

  // Our Impl
  void ConnectWithRetries(const char* socket_name,
                          base::TaskRunner* task_runner);

  // Constructs an instance of a data source of type T.
  template <typename T>
  std::unique_ptr<DimprofdDataSource> CreateDSInstance(
      TracingSessionID session_id,
      const DataSourceConfig& config);

 private:
  static DimprofdProducer* instance_;

  enum State {
    kNotStarted = 0,
    kNotConnected,
    kConnecting,
    kConnected,
  };

  State state_ = kNotStarted;
  base::TaskRunner* task_runner_ = nullptr;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;
  uint32_t connection_backoff_ms_ = 0;
  const char* producer_sock_name_ = nullptr;

  // Owning map for all active data sources.
  std::unordered_map<DataSourceInstanceID, std::unique_ptr<DimprofdDataSource>>
      data_sources_;
  // Keeps (pointers to) data sources grouped by session id and data source
  // type. The pointers do not own the data sources (they're owned by
  // data_sources_).
  //
  // Used by OnFtraceDataWrittenIntoDataSourceBuffers().
  std::unordered_map<
      TracingSessionID,
      std::unordered_multimap<const DimprofdDataSource::Descriptor*,
                              DimprofdDataSource*>>
      session_data_sources_;

  void ConnectService();
  void RestartProducer();
  void IncreaseConnectionBackoff();
  void ResetConnectionBackoff();
  void OnDataSourceFlushComplete(FlushRequestID, DataSourceInstanceID);
  void OnFlushTimeout(FlushRequestID);

  std::unordered_multimap<FlushRequestID, DataSourceInstanceID>
      pending_flushes_;

  std::function<void()> all_data_sources_registered_cb_;

  base::WeakPtrFactory<DimprofdProducer> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MTK_DIMPROFD_PRODUCER_H_

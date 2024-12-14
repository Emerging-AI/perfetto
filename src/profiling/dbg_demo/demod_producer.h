
#ifndef SRC_PROFILING_DBG_DEMOD_PRODUCER_H_
#define SRC_PROFILING_DBG_DEMOD_PRODUCER_H_

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

#include "protos/perfetto/config/profiling/demod_config.gen.h"

namespace perfetto {
namespace profiling {

using DemodConfig = protos::gen::DemodConfig;

enum class DemodMode { kCentral, kChild };

class DemodProducer : public Producer {
 public:
  friend class SocketDelegate;

  // TODO(fmayer): Split into two delegates for the listening socket in kCentral
  // and for the per-client sockets to make this easier to understand?
  // Alternatively, find a better name for this.
  class SocketDelegate : public base::UnixSocket::EventListener {
   public:
    explicit SocketDelegate(DemodProducer* producer) : producer_(producer) {}

    void OnDisconnect(base::UnixSocket* self) override;
    void OnNewIncomingConnection(
        base::UnixSocket* self,
        std::unique_ptr<base::UnixSocket> new_connection) override;
    void OnDataAvailable(base::UnixSocket* self) override;

   private:
    DemodProducer* producer_;
  };

  //
  DemodProducer(DemodMode mode,
                base::TaskRunner* task_runner,
                bool exit_when_done);
  ~DemodProducer() override;

  // Producer Impl:
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(FlushRequestID,
             const DataSourceInstanceID* data_source_ids,
             size_t num_data_sources,
             FlushFlags) override;
  void ClearIncrementalState(const DataSourceInstanceID* /*data_source_ids*/,
                             size_t /*num_data_sources*/) override {}

  // TODO(fmayer): Refactor once/if we have generic reconnect logic.
  void ConnectWithRetries(const char* socket_name);

 private:
  // State of the connection to tracing service (traced).
  enum State {
    kNotStarted = 0,
    kNotConnected,
    kConnecting,
    kConnected,
  };


  struct DataSource {
    // TODO
    explicit DataSource(std::unique_ptr<TraceWriter> tw)
        : trace_writer(std::move(tw)) {}

    DataSourceInstanceID id;
    std::unique_ptr<TraceWriter> trace_writer;
    DemodConfig config;

  };

  struct PendingProcess {
    std::unique_ptr<base::UnixSocket> sock;
    DataSourceInstanceID data_source_instance_id;
  };

  // Class state:
  base::TaskRunner* const task_runner_;
  const DemodMode mode_;
  // TODO(fmayer): Refactor to make this boolean unnecessary.
  // Whether to terminate this producer after the first data-source has
  // finished.
  bool exit_when_done_;

  // State of connection to the tracing service.
  State state_ = kNotStarted;
  uint32_t connection_backoff_ms_ = 0;
  const char* producer_sock_name_ = nullptr;

  // Client processes that have connected, but with which we have not yet
  // finished the handshake.
  std::map<pid_t, PendingProcess> pending_processes_;

  // Must outlive data_sources_ - owns at least the shared memory referenced by
  // TraceWriters.
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;

  // Must outlive data_sources_ - DataSource can hold
  // SystemProperties::Handle-s.
  // Specific to mode_ == kCentral
  // SystemProperties properties_;

  std::map<FlushRequestID, size_t> flushes_in_progress_;
  std::map<DataSourceInstanceID, DataSource> data_sources_;

  //  Specific to mode_ == kChild
  // Process target_process_{base::kInvalidPid, ""};
  // std::optional<std::function<void()>> data_source_callback_;

  // Exposed for testing.
  void SetProducerEndpoint(
      std::unique_ptr<TracingService::ProducerEndpoint> endpoint);

  SocketDelegate socket_delegate_;

  base::WeakPtrFactory<DemodProducer> weak_factory_;


  void ConnectService();
  void ResetConnectionBackoff();

  void DumpMyInfosInDataSource(DataSource* ds);

  void DoDrainAndContinuousDump(DataSourceInstanceID id);
  void DoContinuousDump(DataSource* ds);


};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_DBG_DEMOD_PRODUCER_H_

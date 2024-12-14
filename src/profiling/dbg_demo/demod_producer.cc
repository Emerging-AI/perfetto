
#include "src/profiling/dbg_demo/demod_producer.h"

#include <signal.h>
#include <unistd.h>

#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_demod_packet.pbzero.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <sys/system_properties.h>
#endif

namespace perfetto {
namespace profiling {

namespace {

using ::perfetto::protos::pbzero::ProfileDemodPacket;

constexpr char kDemodDataSource[] = "perfetto.demod";

constexpr uint32_t kInitialConnectionBackoffMs = 100;
// constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;

// Constants specified by bionic, hardcoded here for simplicity.
// constexpr int kProfilingSignal = __SIGRTMIN + 4;
// constexpr int kDemodSignalValue = 0;

}  // namespace

// sample funcions

void DemodProducer::DoDrainAndContinuousDump(DataSourceInstanceID ds_id) {
  // do something you need
  auto it = data_sources_.find(ds_id);
  if (it == data_sources_.end())
    return;

  PERFETTO_LOG("Do something with data source %" PRIu64, ds_id);
  DataSource& ds = it->second;
  DoContinuousDump(&ds);
}

void DemodProducer::DoContinuousDump(DataSource* ds) {

  DumpMyInfosInDataSource(ds); // 最好放到DataSource下，Tick/Sample/trace

  auto ds_id = ds->id;
  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer, ds_id] {
        if (!weak_producer)
          return;
        weak_producer->DoDrainAndContinuousDump(ds_id);
      },
      1000);
}

void DemodProducer::DumpMyInfosInDataSource(DataSource* ds) {
  uint64_t gpu_freq_val = 12345; // TODO: get the values by fd reader

  TraceWriter* tw = ds->trace_writer.get();
  auto packet = tw->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));
  ProfileDemodPacket* demod_packet = packet->set_profile_demod_packet();
  auto* myinfos_dumps = demod_packet->add_myinfos_dumps();
  myinfos_dumps->set_gpu_freq_counter(gpu_freq_val);

}


// -----------
DemodProducer::DemodProducer(DemodMode mode,
                             base::TaskRunner* task_runner,
                             bool exit_when_done)
    : task_runner_(task_runner),
      mode_(mode),
      exit_when_done_(exit_when_done),
      socket_delegate_(this),
      weak_factory_(this) {
  PERFETTO_LOG("Create a DemodProducer");
  // Do some check
  //   CheckDataSourceCpuTask();
  //   CheckDataSourceMemoryTask();
}

DemodProducer::~DemodProducer() = default;

// socket_delegate impl
void DemodProducer::SocketDelegate::OnDisconnect(base::UnixSocket*) {
  PERFETTO_LOG("SocketDelegate::OnDisconnect %d",
               producer_->connection_backoff_ms_);
}
void DemodProducer::SocketDelegate::OnNewIncomingConnection(
    base::UnixSocket*,
    std::unique_ptr<base::UnixSocket>) {
  PERFETTO_LOG("SocketDelegate::OnNewIncomingConnection");
}
void DemodProducer::SocketDelegate::OnDataAvailable(base::UnixSocket*) {
  PERFETTO_LOG("SocketDelegate::OnDataAvailable");
}

// producor impl
void DemodProducer::OnConnect() {
  PERFETTO_LOG("DemodProducer::OnConnect");
  PERFETTO_DCHECK(state_ == kConnecting);
  state_ = kConnected;
  ResetConnectionBackoff();
  PERFETTO_LOG("Connected to the service, mode [%s].",
               mode_ == DemodMode::kCentral ? "central" : "child");

  {
    // custom
    DataSourceDescriptor desc;
    desc.set_name(kDemodDataSource);
    desc.set_will_notify_on_stop(true);
    endpoint_->RegisterDataSource(desc);
  }
}

void DemodProducer::OnDisconnect() {
  PERFETTO_LOG("DemodProducer::OnDisconnect");
}
void DemodProducer::SetupDataSource(DataSourceInstanceID ds_id,
                                    const DataSourceConfig& ds_config) {
  PERFETTO_LOG("DemodProducer::SetupDataSource");

  if (data_sources_.find(ds_id) != data_sources_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Duplicate data source: %" PRIu64, ds_id);
    return;
  }

  DemodConfig config;
  config.ParseFromString(ds_config.demod_config_raw());
  // check config
  // TODO:

  auto buffer_id = static_cast<BufferID>(ds_config.target_buffer());
  DataSource data_source(endpoint_->CreateTraceWriter(buffer_id));
  data_source.id = ds_id;
  data_source.config = config;
  data_sources_.emplace(ds_id, std::move(data_source));
  PERFETTO_LOG("Set up data source.");
}
void DemodProducer::StartDataSource(DataSourceInstanceID ds_id,
                                    const DataSourceConfig& config) {
  PERFETTO_LOG("DemodProducer::StartDataSource");
  uint64_t tracing_session_id = config.tracing_session_id();
  PERFETTO_LOG("StartDataSource(ds %zu, session %" PRIu64 ", name %s)",
               static_cast<size_t>(ds_id), tracing_session_id,
               config.name().c_str());

  auto it = data_sources_.find(ds_id);
  if (it == data_sources_.end()) {
    PERFETTO_ELOG("data_sources_ not found: %" PRIu64, ds_id);
    return;
  }

  // if (config.name() != kDemodDataSource)
  //   return;

  auto weak_producer = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_producer, ds_id] {
        if (!weak_producer)
          return;
        weak_producer->DoDrainAndContinuousDump(ds_id);
      },
      1000);
}

void DemodProducer::StopDataSource(DataSourceInstanceID ds_id) {
  PERFETTO_LOG("DemodProducer::StopDataSource");
  auto it = data_sources_.find(ds_id);
  if (it == data_sources_.end()) {
    endpoint_->NotifyDataSourceStopped(ds_id);
    PERFETTO_ELOG("Trying to stop non existing data source: %" PRIu64, ds_id);
    return;
  }

  PERFETTO_LOG("Stopping data source %" PRIu64, ds_id);

  // DataSource& data_source = it->second;
  data_sources_.erase(it);
}

void DemodProducer::OnTracingSetup() {
  PERFETTO_LOG("DemodProducer::OnTracingSetup");
}
void DemodProducer::Flush(FlushRequestID flush_id,
                          const DataSourceInstanceID* data_source_ids,
                          size_t num_data_sources,
                          FlushFlags) {
  PERFETTO_LOG("DemodProducer::Flush");
  for (size_t i = 0; i < num_data_sources; i++) {
    auto ds_id = data_source_ids[i];
    PERFETTO_DLOG("Flush(%zu)", static_cast<size_t>(ds_id));
  }

  endpoint_->NotifyFlushComplete(flush_id);
  // PERFETTO_LOG("%s", data_source_ids);
  // PERFETTO_LOG("%d", num_data_sources);
}

void DemodProducer::ConnectWithRetries(const char* socket_name) {
  PERFETTO_DCHECK(state_ == kNotStarted);
  state_ = kNotConnected;

  ResetConnectionBackoff();
  producer_sock_name_ = socket_name;
  ConnectService();
}

// private method

void DemodProducer::ConnectService() {
  PERFETTO_LOG("DemodProducer::ConnectService");
  SetProducerEndpoint(ProducerIPCClient::Connect(
      producer_sock_name_, this, kDemodDataSource, task_runner_));
}

void DemodProducer::SetProducerEndpoint(
    std::unique_ptr<TracingService::ProducerEndpoint> endpoint) {
  PERFETTO_LOG("DemodProducer::SetProducerEndpoint");
  PERFETTO_DCHECK(state_ == kNotConnected || state_ == kNotStarted);
  state_ = kConnecting;
  endpoint_ = std::move(endpoint);
}

void DemodProducer::ResetConnectionBackoff() {
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
}

}  // namespace profiling
}  // namespace perfetto

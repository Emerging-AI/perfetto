#include "src/profiling/mtk/dimprofd_producer.h"

#include <unistd.h>

#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"

#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_stats_data_source.h"
#include "src/profiling/mtk/network_stats/network_stats_data_source.h"

namespace perfetto {
namespace profiling {
  

namespace {

constexpr uint32_t kInitialConnectionBackoffMs = 100;
constexpr uint32_t kMaxConnectionBackoffMs = 30 * 1000;

// Should be larger than FtraceController::kControllerFlushTimeoutMs.
constexpr uint32_t kFlushTimeoutMs = 1000;

}  // namespace

// State transition diagram:
//                    +----------------------------+
//                    v                            +
// NotStarted -> NotConnected -> Connecting -> Connected
//                    ^              +
//                    +--------------+
//

DimprofdProducer* DimprofdProducer::instance_ = nullptr;

DimprofdProducer* DimprofdProducer::GetInstance() {
  return instance_;
}

DimprofdProducer::DimprofdProducer() : weak_factory_(this) {
  PERFETTO_CHECK(instance_ == nullptr);
  instance_ = this;
}

DimprofdProducer::~DimprofdProducer() {
  instance_ = nullptr;
  data_sources_.clear();
}

template <>
std::unique_ptr<DimprofdDataSource>
DimprofdProducer::CreateDSInstance<ArmGpuStatsDataSource>(
    TracingSessionID session_id,
    const DataSourceConfig& config) {
  auto buffer_id = static_cast<BufferID>(config.target_buffer());
  return std::unique_ptr<DimprofdDataSource>(new ArmGpuStatsDataSource(
      task_runner_, session_id, endpoint_->CreateTraceWriter(buffer_id), config));
}

template <>
std::unique_ptr<DimprofdDataSource>
DimprofdProducer::CreateDSInstance<NetworkStatsDataSource>(
    TracingSessionID session_id,
    const DataSourceConfig& config) {
  auto buffer_id = static_cast<BufferID>(config.target_buffer());
  return std::unique_ptr<DimprofdDataSource>(new NetworkStatsDataSource(
      task_runner_, session_id, endpoint_->CreateTraceWriter(buffer_id), config));
}

// Another anonymous namespace. This cannot be moved into the anonymous
// namespace on top (it would fail to compile), because the CreateDSInstance
// methods need to be fully declared before.
// basically for datasource descriptors
namespace {

using DimprofdDataSourceFactoryFunc = std::unique_ptr<DimprofdDataSource> (
    DimprofdProducer::*)(TracingSessionID, const DataSourceConfig&);

struct DataSourceTraits {
  const DimprofdDataSource::Descriptor* descriptor;
  DimprofdDataSourceFactoryFunc factory_func;
};

template <typename T>
constexpr DataSourceTraits Ds() {
  return DataSourceTraits{&T::descriptor,
                          &DimprofdProducer::CreateDSInstance<T>};
}

constexpr const DataSourceTraits kAllDataSources[] = {
    Ds<ArmGpuStatsDataSource>(),
    Ds<NetworkStatsDataSource>(),
};

}  // namespace

// Producer Impl:
void DimprofdProducer::OnConnect() {
  // kConnecting -> kConnected
  PERFETTO_DCHECK(state_ == kConnecting);
  state_ = kConnected;
  ResetConnectionBackoff();
  PERFETTO_LOG("Connected to the service");

  // 1. generate datasource descriptors
  std::array<DataSourceDescriptor, base::ArraySize(kAllDataSources)>
      proto_descs;
  for (size_t i = 0; i < proto_descs.size(); i++) {
    DataSourceDescriptor& proto_desc = proto_descs[i];
    const DimprofdDataSource::Descriptor* desc = kAllDataSources[i].descriptor;
    // check the Duplicate, if Duplicate then crash.
    for (size_t j = i + 1; j < proto_descs.size(); j++) {
      if (kAllDataSources[i].descriptor == kAllDataSources[j].descriptor) {
        PERFETTO_FATAL("Duplicate descriptor name %s",
                       kAllDataSources[i].descriptor->name);
      }
    }

    proto_desc.set_name(desc->name);
    proto_desc.set_will_notify_on_start(true);
    proto_desc.set_will_notify_on_stop(true);
    using Flags = DimprofdDataSource::Descriptor::Flags;
    if (desc->flags & Flags::kHandlesIncrementalState)
      proto_desc.set_handles_incremental_state_clear(true);
    if (desc->fill_descriptor_func) {
      desc->fill_descriptor_func(&proto_desc);
    }
  }

  // 2. register datasource
  for (const DataSourceDescriptor& proto_desc : proto_descs) {
    endpoint_->RegisterDataSource(proto_desc);
  }

  // Used by tracebox to synchronize with traced_probes being registered.
  if (all_data_sources_registered_cb_) {
    endpoint_->Sync(all_data_sources_registered_cb_);
  }
};

void DimprofdProducer::OnDisconnect() {
  // kConnected -> kNotConnected (restart)
  // kConnecting -> kNotConnected (reconnect)
  PERFETTO_DCHECK(state_ == kConnected || state_ == kConnecting);
  PERFETTO_LOG("Disconnected from tracing service");
  if (state_ == kConnected)
    return task_runner_->PostTask([this] { this->RestartProducer(); });

  state_ = kNotConnected;
  IncreaseConnectionBackoff();
  task_runner_->PostDelayedTask([this] { this->ConnectService(); },
                                connection_backoff_ms_);
};

void DimprofdProducer::SetupDataSource(DataSourceInstanceID instance_id,
                                       const DataSourceConfig& config) {
  PERFETTO_LOG("SetupDataSource(id=%" PRIu64 ", name=%s)", instance_id,
                config.name().c_str());
  PERFETTO_DCHECK(data_sources_.count(instance_id) == 0);
  TracingSessionID session_id = config.tracing_session_id();
  PERFETTO_CHECK(session_id > 0);

  std::unique_ptr<DimprofdDataSource> data_source;

  for (const DataSourceTraits& rds : kAllDataSources) {
    if (rds.descriptor->name != config.name()) {
      continue;
    }
    data_source = (this->*(rds.factory_func))(session_id, config);
    break;
  }

  if (!data_source) {
    PERFETTO_ELOG("Failed to create data source '%s'", config.name().c_str());
    return;
  }
  session_data_sources_[session_id].emplace(data_source->descriptor,
                                            data_source.get());
  data_sources_[instance_id] = std::move(data_source);
};

void DimprofdProducer::StartDataSource(DataSourceInstanceID instance_id,
                                       const DataSourceConfig& config) {
  PERFETTO_LOG("StartDataSource(id=%" PRIu64 ", name=%s)", instance_id,
                config.name().c_str());
  auto it = data_sources_.find(instance_id);
  if (it == data_sources_.end()) {
    // Can happen if SetupDataSource() failed (e.g. ftrace was busy).
    PERFETTO_ELOG("Data source id=%" PRIu64 " not found", instance_id);
    return;
  }
  DimprofdDataSource* data_source = it->second.get();
  if (data_source->started)
    return;

  // NOTE: traced_probes need to handle "config.trace_duration_ms() != 0" using watchdogs_
  // but dimprofd always works as daemon
  // TODO: why using watchdogs_?
  
  data_source->started = true;
  data_source->Start();
  endpoint_->NotifyDataSourceStarted(instance_id);
};

void DimprofdProducer::StopDataSource(DataSourceInstanceID instance_id){
  PERFETTO_LOG("Producer stop (id=%" PRIu64 ")", instance_id);
  auto it = data_sources_.find(instance_id);
  if (it == data_sources_.end()) {
    // Can happen if SetupDataSource() failed (e.g. ftrace was busy).
    PERFETTO_ELOG("Cannot stop data source id=%" PRIu64 ", not found", instance_id);
    return;
  }
  DimprofdDataSource* data_source = it->second.get();

  // TODO: need to understand Metatrace, that use to improve the performence of trace_writer
  // MetatraceDataSource special case: re-flush to record the final flushes of
  // other data sources.
  // if (data_source->descriptor == &MetatraceDataSource::descriptor)
  //   data_source->Flush(FlushRequestID{0}, [] {});

  TracingSessionID session_id = data_source->tracing_session_id;

  auto session_it = session_data_sources_.find(session_id);
  if (session_it != session_data_sources_.end()) {
    auto desc_range = session_it->second.equal_range(data_source->descriptor);
    for (auto ds_it = desc_range.first; ds_it != desc_range.second; ds_it++) {
      if (ds_it->second == data_source) {
        session_it->second.erase(ds_it);
        if (session_it->second.empty()) {
          session_data_sources_.erase(session_it);
        }
        break;
      }
    }
  }
  data_sources_.erase(it);
  // watchdogs_.erase(id);

  // We could (and used to) acknowledge the stop before tearing the local state
  // down, allowing the tracing service and the consumer to carry on quicker.
  // However in the case of tracebox, the traced_probes subprocess gets killed
  // as soon as the trace is considered finished (i.e. all data source stops
  // were acked), and therefore the kill would race against the tracefs
  // cleanup.
  endpoint_->NotifyDataSourceStopped(instance_id);

};

void DimprofdProducer::OnTracingSetup() {};

void DimprofdProducer::Flush(FlushRequestID flush_request_id,
                             const DataSourceInstanceID* data_source_ids,
                             size_t num_data_sources,
                             FlushFlags /*flush_flags*/) {
  PERFETTO_LOG("ProbesProducer::Flush(%" PRIu64 ") begin", flush_request_id);
  PERFETTO_DCHECK(flush_request_id);
  auto log_on_exit = base::OnScopeExit([&] {
    PERFETTO_LOG("ProbesProducer::Flush(%" PRIu64 ") end", flush_request_id);
  });

  // Flush() to all started data sources.
  std::vector<std::pair<DataSourceInstanceID, DimprofdDataSource*>> ds_to_flush;
  for (size_t i = 0; i < num_data_sources; i++) {
    DataSourceInstanceID ds_id = data_source_ids[i];
    auto it = data_sources_.find(ds_id);
    if (it == data_sources_.end() || !it->second->started)
      continue;
    pending_flushes_.emplace(flush_request_id, ds_id);
    ds_to_flush.emplace_back(std::make_pair(ds_id, it->second.get()));
  }

  // If there is nothing to flush, ack immediately.
  if (ds_to_flush.empty()) {
    endpoint_->NotifyFlushComplete(flush_request_id);
    return;
  }

  // Otherwise post the timeout task and issue all flushes in order.
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this, flush_request_id] {
        if (weak_this)
          weak_this->OnFlushTimeout(flush_request_id);
      },
      kFlushTimeoutMs);

  // Issue all the flushes in order. We do this in a separate loop to deal with
  // the case of data sources invoking the callback synchronously (b/295189870).
  for (const auto& kv : ds_to_flush) {
    const DataSourceInstanceID ds_id = kv.first;
    DimprofdDataSource* const data_source = kv.second;
    auto flush_callback = [weak_this, flush_request_id, ds_id] {
      if (weak_this)
        weak_this->OnDataSourceFlushComplete(flush_request_id, ds_id);
    };
    PERFETTO_LOG("Flushing data source %" PRIu64 " %s", ds_id,
                  data_source->descriptor->name);
    data_source->Flush(flush_request_id, flush_callback);
  }
};

void DimprofdProducer::OnDataSourceFlushComplete(
    FlushRequestID flush_request_id,
    DataSourceInstanceID ds_id) {
  PERFETTO_LOG("Flush %" PRIu64 " acked by data source %" PRIu64,
                flush_request_id, ds_id);
  auto range = pending_flushes_.equal_range(flush_request_id);
  for (auto it = range.first; it != range.second; it++) {
    if (it->second == ds_id) {
      pending_flushes_.erase(it);
      break;
    }
  }

  if (pending_flushes_.count(flush_request_id))
    return;  // Still waiting for other data sources to ack.

  PERFETTO_LOG("All data sources acked to flush %" PRIu64, flush_request_id);
  endpoint_->NotifyFlushComplete(flush_request_id);
}

void DimprofdProducer::OnFlushTimeout(FlushRequestID flush_request_id) {
  if (pending_flushes_.count(flush_request_id) == 0)
    return;  // All acked.
  PERFETTO_ELOG("Flush(%" PRIu64 ") timed out", flush_request_id);
  pending_flushes_.erase(flush_request_id);
  endpoint_->NotifyFlushComplete(flush_request_id);
}

void DimprofdProducer::ClearIncrementalState(
    const DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  for (size_t i = 0; i < num_data_sources; i++) {
    DataSourceInstanceID ds_id = data_source_ids[i];
    auto it = data_sources_.find(ds_id);
    if (it == data_sources_.end() || !it->second->started)
      continue;

    it->second->ClearIncrementalState();
  }
};

// Our Impl:
void DimprofdProducer::ConnectWithRetries(const char* socket_name,
                                          base::TaskRunner* task_runner) {
  // kNotStarted -> kNotConnected
  PERFETTO_DCHECK(state_ == kNotStarted);
  state_ = kNotConnected;

  ResetConnectionBackoff();
  producer_sock_name_ = socket_name;
  task_runner_ = task_runner;
  ConnectService();
}

// private method Impl:
void DimprofdProducer::ConnectService() {
  // kNotConnected -> kConnecting
  PERFETTO_DCHECK(state_ == kNotConnected);
  state_ = kConnecting;
  endpoint_ = ProducerIPCClient::Connect(producer_sock_name_, this,
                                         "mtk.dimprofd", task_runner_);
}

void DimprofdProducer::RestartProducer() {
  // We lost the connection with the tracing service. At this point we need
  // to reset all the data sources. Trying to handle that manually is going to
  // be error prone. What we do here is simply destroying the instance and
  // recreating it again.

  base::TaskRunner* task_runner = task_runner_;
  const char* socket_name = producer_sock_name_;

  // Invoke destructor and then the constructor again.
  this->~DimprofdProducer();
  new (this) DimprofdProducer();

  ConnectWithRetries(socket_name, task_runner);
}

void DimprofdProducer::IncreaseConnectionBackoff() {
  connection_backoff_ms_ *= 2;
  if (connection_backoff_ms_ > kMaxConnectionBackoffMs)
    connection_backoff_ms_ = kMaxConnectionBackoffMs;
}

void DimprofdProducer::ResetConnectionBackoff() {
  connection_backoff_ms_ = kInitialConnectionBackoffMs;
}

}  // namespace profiling
}  // namespace perfetto

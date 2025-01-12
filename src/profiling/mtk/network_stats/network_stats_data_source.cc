
#include "src/profiling/mtk/network_stats/network_stats_data_source.h"

#include "src/profiling/common/proc_utils.h"

#include <optional>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

#include "protos/perfetto/config/profiling/network_stats_config.pbzero.h"
#include "protos/perfetto/trace/profiling/network_stats.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

// using protos::pbzero::ArmGpuStatsConfig;

namespace profiling {

namespace {
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
const DimprofdDataSource::Descriptor NetworkStatsDataSource::descriptor = {
    /* name */ "linux.network_stats",
    /* flags */ Descriptor::kFlagsNone,
    /* fill_descriptor_func */ nullptr,
};

NetworkStatsDataSource::~NetworkStatsDataSource() {}

NetworkStatsDataSource::NetworkStatsDataSource(
    base::TaskRunner* task_runner,
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer,
    const DataSourceConfig& ds_config)
    : DimprofdDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  // get config params
  using protos::pbzero::NetworkStatsConfig;
  NetworkStatsConfig::Decoder cfg(ds_config.network_stats_config_raw());
  tick_period_ms_ = ClampTo10Ms(cfg.network_period_ms(), "network_period_ms");

  // TODO(xr): paramalize
  cmdlines_ = {"com.taobao.taobao"};
  // cmdlines_ = {"com.tencent.mm"};
}

void NetworkStatsDataSource::Start() {
  auto weak_this = GetWeakPtr();
  task_runner_->PostTask(std::bind(&NetworkStatsDataSource::Tick, weak_this));
}

// static
void NetworkStatsDataSource::Tick(
    base::WeakPtr<NetworkStatsDataSource> weak_this) {
  if (!weak_this)
    return;
  NetworkStatsDataSource& thiz = *weak_this;

  uint32_t period_ms = thiz.tick_period_ms_;
  uint32_t delay_ms =
      period_ms -
      static_cast<uint32_t>(base::GetWallTimeMs().count() % period_ms);
  thiz.task_runner_->PostDelayedTask(
      std::bind(&NetworkStatsDataSource::Tick, weak_this), delay_ms);
  thiz.ReadNetworkStats();
}

void NetworkStatsDataSource::ReadNetworkStats() {
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));
  protos::pbzero::NetworkStats* network_stats = packet->set_network_stats();

  // Get Specific PID
  pids_.clear();
  FindPidsForCmdlines(cmdlines_, &pids_);
  // glob_aware::FindPidsForCmdlinePatterns(cmdlines_, &pids_);

  // read proc/PID/net/[tcp, tcp6, udp, udp6]
  uint64_t tcp_tb = 0, tcp_rb = 0, udp_tb = 0, udp_rb = 0;
  for(auto iter = pids_.begin(); iter != pids_.end(); ++iter){
    pid_t pid = *iter;
    ReadNetworkInfo("tcp", pid, tcp_tb, tcp_rb);
    ReadNetworkInfo("tcp6", pid, tcp_tb, tcp_rb);
    ReadNetworkInfo("udp", pid, udp_tb, udp_rb);
    ReadNetworkInfo("udp6", pid, udp_tb, udp_rb);

    PERFETTO_LOG("PID: %d", pid);
  }
  
  network_stats->set_tcp_tb(tcp_tb);
  network_stats->set_tcp_rb(tcp_rb);
  network_stats->set_tcp_total(tcp_tb + tcp_rb);
  network_stats->set_udp_tb(udp_tb);
  network_stats->set_udp_rb(udp_rb);
  network_stats->set_udp_total(udp_tb + udp_rb);

  PERFETTO_LOG("TCP - tb : rb - %lu : %lu", tcp_tb, tcp_rb);
  PERFETTO_LOG("UDP - tb : rb - %lu : %lu", udp_tb, udp_rb);
}

void NetworkStatsDataSource::ReadNetworkInfo(const std::string& protocol, const pid_t pid,
                                             uint64_t& tb, uint64_t& rb){
  std::string content = ReadFile("/proc/" + std::to_string(pid) + "/net/" + protocol);
  if (content.empty()) {
    return;
  }
  std::string::iterator line_start = content.begin();
  std::string::iterator line_end = content.end();

  bool bTitleRead = false;
  while (line_start != content.end()) {
    line_end = find(line_start, content.end(), '\n');
    if (line_end == content.end())
      break;
    std::string line = std::string(line_start, line_end);
    line_start = line_end + 1;
    // skip Title
    if (!bTitleRead) {
      bTitleRead = true;
      continue;
    }
    // === Title Format ===
    // sl local_address rem_address st tx_queue:rx_queue tr tm->when retrnsmt
    // uid  timeout inode ref pointer drops
    // ====================
    auto splits = base::SplitString(line, " ");
    if (splits.size() < 5) {
      continue;
    }

    auto tx_rx = base::SplitString(splits[4], ":");
    if (tx_rx.size() < 2) {
      continue;
    }

    // PERFETTO_LOG("proc/net/tcp6 - Read lines - %s", line.c_str());
    // PERFETTO_LOG("proc/net/tcp6 - Read words - 4 - %s", splits[4].c_str());
    uint64_t tx = static_cast<uint64_t>(strtoll(tx_rx[0].c_str(), nullptr, 16));
    uint64_t rx = static_cast<uint64_t>(strtoll(tx_rx[1].c_str(), nullptr, 16));
    tb += tx;
    rb += rx;
    // PERFETTO_LOG("proc/net/tcp6 - tx : rx - %lu : %lu", tx, rx);
  }
}

base::WeakPtr<NetworkStatsDataSource> NetworkStatsDataSource::GetWeakPtr()
    const {
  return weak_factory_.GetWeakPtr();
}

void NetworkStatsDataSource::Flush(FlushRequestID,
                                   std::function<void()> callback) {
  PERFETTO_LOG("Flush time %ld",
               static_cast<uint64_t>(base::GetBootTimeNs().count()));
  writer_->Flush(callback);
}

std::string NetworkStatsDataSource::ReadFile(const std::string& path) {
  std::string contents;
  if (!base::ReadFile(path, &contents)) {
    PERFETTO_PLOG("Failed reading %s", path.c_str());
    return "";
  }
  return contents;
}

}  // namespace profiling
}  // namespace perfetto

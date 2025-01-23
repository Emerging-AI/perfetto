
#include "src/trace_processor/importers/proto/dimprofd_probes_parser.h"

#include "perfetto/base/logging.h"
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_counters.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"
#include "protos/perfetto/trace/profiling/network_stats.pbzero.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

namespace {}  // namespace

DimprofdProbesParser::DimprofdProbesParser(TraceProcessorContext* context)
    : context_(context) {
  for (const auto& name :  profiling::BuildArmGpuinfoCounterNames()) {
    arm_gpuinfo_strs_id_.emplace_back(context->storage->InternString(name));
  }
}

void DimprofdProbesParser::ParseArmGpuStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::ArmGpuStats::Decoder arm_gpu_stats(blob.data, blob.size);

  for (auto it = arm_gpu_stats.arm_gpuinfo(); it; ++it) {
    protos::pbzero::ArmGpuStats::ArmGpuInfoValue::Decoder gi(*it);
    auto key = static_cast<size_t>(gi.key());
    if (PERFETTO_UNLIKELY(key >= arm_gpuinfo_strs_id_.size())) {
      PERFETTO_ELOG("ArmGpuInfo key %zu is not recognized.", key);
      context_->storage->IncrementStats(stats::arm_gpuinfo_unknown_keys);
      continue;
    }


    // hwcpipe counters
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        TrackTracker::Group::kGpu, arm_gpuinfo_strs_id_[key]);
    // gi.val_type just equal 0 or 1
    PERFETTO_DLOG("\nArmGpuInfo val_type %u ", gi.val_type());
    if (gi.val_type() == 0) {
      PERFETTO_DLOG("\nArmGpuInfo %zu int value %llu", key, gi.int_value());
      context_->event_tracker->PushCounter(
          ts, static_cast<double>(gi.int_value()), track);
    } else {
      PERFETTO_DLOG("\nArmGpuInfo %zu double value %f", key, gi.double_value());
      context_->event_tracker->PushCounter(
          ts, gi.double_value(), track);
    }

  } 

  for (auto it = arm_gpu_stats.gpufreq_hz(); it; ++it) {
    auto value = static_cast<double>(*it);
    PERFETTO_DLOG("\nArmGpu freq value %f", value);
    StringId name = context_->storage->InternString("mali0_freq");
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        TrackTracker::Group::kGpu, name);
    context_->event_tracker->PushCounter(
        ts, value, track);
  }

  for (auto it = arm_gpu_stats.gpufreq_v2_hz(); it; ++it) {
    auto value = static_cast<double>(*it);
    PERFETTO_DLOG("\nArmGpu freq value %f", value);
    StringId name = context_->storage->InternString("mali0_freq_v2");
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        TrackTracker::Group::kGpu, name);
    context_->event_tracker->PushCounter(
        ts, value, track);
  }
}

void DimprofdProbesParser::ParseNetworkStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::NetworkStats::Decoder network_stats(blob.data, blob.size);

  static const std::vector<std::string> labels = {
    "tcp_rb",
    "tcp_tb",
    "tcp_total",
    "udp_rb",
    "udp_tb",
    "udp_total"
  };

  std::vector<uint64_t> values;
  values.emplace_back(network_stats.tcp_rb());
  values.emplace_back(network_stats.tcp_tb());
  values.emplace_back(network_stats.tcp_total());
  values.emplace_back(network_stats.udp_rb());
  values.emplace_back(network_stats.udp_tb());
  values.emplace_back(network_stats.udp_total());

  for(size_t i=0; i<values.size(); ++i) {
    std::string label = labels[i];
    uint64_t value = values[i];
    StringId name = context_->storage->InternString(label.c_str());
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
      TrackTracker::Group::kNetwork, name);
    context_->event_tracker->PushCounter(ts, value, track);

    PERFETTO_DLOG("\nNetwork %s: %llu", label.c_str(), value);
  }
}
}  // namespace trace_processor
}  // namespace perfetto

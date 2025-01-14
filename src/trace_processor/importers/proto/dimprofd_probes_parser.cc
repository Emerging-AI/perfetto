
#include "src/trace_processor/importers/proto/dimprofd_probes_parser.h"

#include "perfetto/base/logging.h"
#include "src/profiling/mtk/arm_gpu_stats/arm_gpu_counters.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"
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

    PERFETTO_DLOG("\nArmGpuInfo %zu value %llu", key, gi.int_value());

    // hwcpipe counters
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        TrackTracker::Group::kGpu, arm_gpuinfo_strs_id_[key]);
    context_->event_tracker->PushCounter(
        ts, static_cast<double>(gi.int_value()), track);

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
}

}  // namespace trace_processor
}  // namespace perfetto

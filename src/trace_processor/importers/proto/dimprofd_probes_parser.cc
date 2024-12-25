
#include "src/trace_processor/importers/proto/dimprofd_probes_parser.h"

#include "perfetto/base/logging.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {}  // namespace

DimprofdProbesParser::DimprofdProbesParser(TraceProcessorContext* context)
    : context_(context) {}

void DimprofdProbesParser::ParseArmGpuStats(int64_t /*ts*/, ConstBytes blob) {
  protos::pbzero::ArmGpuStats::Decoder arm_gpu_stats(blob.data, blob.size);

  for (auto it = arm_gpu_stats.arm_gpuinfo(); it; ++it) {
    protos::pbzero::ArmGpuStats::ArmGpuInfoValue::Decoder gi(*it);
    auto key = static_cast<size_t>(gi.key());
    PERFETTO_LOG("ArmGpuInfo key %zu", key);
    if (gi.has_int_value()) {
      PERFETTO_LOG("ArmGpuInfo value %llu", gi.int_value());
    }
  }
}

}  // namespace trace_processor
}  // namespace perfetto

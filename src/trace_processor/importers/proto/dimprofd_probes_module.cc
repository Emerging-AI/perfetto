
#include "src/trace_processor/importers/proto/dimprofd_probes_module.h"
#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/importers/proto/dimprofd_probes_parser.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

DimprofdProbesModule::DimprofdProbesModule(TraceProcessorContext* context)
    : parser_(context) {
  RegisterForField(TracePacket::kArmGpuStatsFieldNumber, context);
  RegisterForField(TracePacket::kNetworkStatsFieldNumber, context);
}

void DimprofdProbesModule::ParseTracePacketData(
    const TracePacket::Decoder& decoder,
    int64_t ts,
    const TracePacketData&,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kArmGpuStatsFieldNumber:
      parser_.ParseArmGpuStats(ts, decoder.arm_gpu_stats());
      return;
    case TracePacket::kNetworkStatsFieldNumber:
      parser_.ParseNetworkStats(ts, decoder.network_stats());
      return;
  }
}


}  // namespace trace_processor
}  // namespace perfetto

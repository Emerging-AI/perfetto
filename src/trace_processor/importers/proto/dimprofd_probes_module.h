
#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_MODULE_H_


#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/common/trace_parser.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/dimprofd_probes_parser.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

class DimprofdProbesModule : public ProtoImporterModule {
 public:
  explicit DimprofdProbesModule(TraceProcessorContext* context);


  void ParseTracePacketData(const protos::pbzero::TracePacket::Decoder& decoder,
                            int64_t ts,
                            const TracePacketData&,
                            uint32_t field_id) override;

 private:
  DimprofdProbesParser parser_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_MODULE_H_

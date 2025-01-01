
#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_PARSER_H_

#include <array>
#include <vector>

#include "perfetto/protozero/field.h"
#include "protos/perfetto/trace/profiling/arm_gpu_stats.pbzero.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class DimprofdProbesParser {
 public:
  using ConstBytes = protozero::ConstBytes;

  explicit DimprofdProbesParser(TraceProcessorContext*);

  void ParseArmGpuStats(int64_t ts, ConstBytes);

  TraceProcessorContext* const context_;

 private:

 std::vector<StringId> arm_gpuinfo_strs_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_DIMPROFD_PROBES_PARSER_H_

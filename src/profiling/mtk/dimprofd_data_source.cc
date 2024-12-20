#include "src/profiling/mtk/dimprofd_data_source.h"

namespace perfetto {
namespace profiling {

DimprofdDataSource::DimprofdDataSource(TracingSessionID session_id,
                                       const Descriptor* desc)
    : tracing_session_id(session_id), descriptor(desc) {}

DimprofdDataSource::~DimprofdDataSource() = default;

}  // namespace profiling
}  // namespace perfetto

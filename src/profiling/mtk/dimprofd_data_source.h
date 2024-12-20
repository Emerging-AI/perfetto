
#ifndef SRC_PROFILING_MTK_DIMPROFD_DATA_SOURCE_H_
#define SRC_PROFILING_MTK_DIMPROFD_DATA_SOURCE_H_

#include <functional>

#include "perfetto/base/logging.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/tracing/core/forward_decls.h"

namespace perfetto {
namespace profiling {

// Base class for all data sources in dimprofd.
class DimprofdDataSource {
 public:
  // Static properties for a data source. Needs to be available before
  // instantiating each data source. It must have static lifetime.
  struct Descriptor {
    using FillDescriptorFunc = void (*)(DataSourceDescriptor*);
    enum Flags : uint32_t {
      kFlagsNone = 0,
      kHandlesIncrementalState = 1 << 0,
    };
    const char* const name;
    uint32_t flags;
    // If not nullptr, called to fill data source specific fields in
    // DataSourceDescriptor.
    FillDescriptorFunc fill_descriptor_func;
  };

  DimprofdDataSource(TracingSessionID, const Descriptor*);
  virtual ~DimprofdDataSource();

  virtual void Start() = 0;
  virtual void Flush(FlushRequestID, std::function<void()> callback) = 0;

  // Only data sources that opt in via DataSourceDescriptor should receive this
  // call.
  virtual void ClearIncrementalState() {
    PERFETTO_ELOG(
        "ClearIncrementalState received by data source that doesn't provide "
        "its own implementation.");
  }

  const TracingSessionID tracing_session_id;
  const Descriptor* const descriptor;
  bool started = false;  // Set by probes_producer.cc.

 private:
  DimprofdDataSource(const DimprofdDataSource&) = delete;
  DimprofdDataSource& operator=(const DimprofdDataSource&) = delete;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MTK_DIMPROFD_DATA_SOURCE_H_



#ifndef SRC_PROFILING_DBG_WIRE_PROTOCOL_H_
#define SRC_PROFILING_DBG_WIRE_PROTOCOL_H_

namespace perfetto {

namespace base {
class UnixSocketRaw;
}

namespace profiling {


constexpr const char* kDemodSocketEnvVar = "ANDROID_SOCKET_demod";
constexpr const char* kDemodSocketFile = "/dev/socket/demod";

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_DBG_WIRE_PROTOCOL_H_

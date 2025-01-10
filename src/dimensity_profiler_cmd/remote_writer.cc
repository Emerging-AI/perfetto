#include "src/dimensity_profiler_cmd/remote_writer.h"
#include <array>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include "perfetto/base/logging.h"
#include "protos/perfetto/remote_writer/remote_writer.pb.h"
#include "perfetto/protozero/proto_utils.h"
#include "protos/perfetto/trace/trace.pbzero.h"
namespace perfetto {
namespace {
using protozero::proto_utils::MakeTagLengthDelimited;
using protozero::proto_utils::WriteVarInt;
using Preamble = std::array<char, 16>;
template <uint32_t id>
size_t GetPreamble(size_t sz, Preamble* preamble) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(preamble->data());
  constexpr uint32_t tag = MakeTagLengthDelimited(id);
  ptr = WriteVarInt(tag, ptr);
  ptr = WriteVarInt(sz, ptr);
  size_t preamble_size = reinterpret_cast<uintptr_t>(ptr) -
                         reinterpret_cast<uintptr_t>(preamble->data());
  PERFETTO_DCHECK(preamble_size < preamble->size());
  return preamble_size;
}
}  // namespace
RemoteWriter::RemoteWriter(std::string host, int port) {
    host_ = host;
    port_ = port;
    std::string address = host + ":" + std::to_string(port);
    channel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    stub_ = protos::TraceDataService::NewStub(channel_);
}
RemoteWriter::~RemoteWriter() {
    
}
bool RemoteWriter::WritePacket(const TracePacket& packet) {
    if (packet.slices().size() == 0) {
        return true;
    }
    std::vector<char> packet_data;
    Preamble preamble;
    size_t size = GetPreamble<protos::pbzero::Trace::kPacketFieldNumber>(
        packet.size(), &preamble);
    packet_data.insert(packet_data.end(), preamble.data(), preamble.data() + size);
    for (const Slice& slice : packet.slices()) {
        const char* slice_data = reinterpret_cast<const char*>(slice.start);
        packet_data.insert(packet_data.end(), slice_data, slice_data + slice.size);
    }
    // 构建 gRPC 请求
    protos::TraceDataRequest request;
    request.set_data(packet_data.data(), packet_data.size());
    request.set_size(static_cast<int32_t>(packet_data.size()));
    grpc::ClientContext context;
    protos::TraceDataResponse response;
    grpc::Status status = stub_->SendTraceData(&context, request, &response);
    if (!status.ok()) {
        PERFETTO_LOG("WritePacket status not ok, msg: %s", status.error_message().c_str());
        return false;
    }
    if (response.status() != "success") {
        PERFETTO_LOG("WritePacket return status not success");
        return false;
    }
    return true;
}
} // namespace perfetto
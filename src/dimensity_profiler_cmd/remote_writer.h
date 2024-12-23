/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SRC_PERFETTO_CMD_REMOTE_WRITER_H_
#define SRC_PERFETTO_CMD_REMOTE_WRITER_H_
#include <vector>
#include <stdio.h>
#include <grpcpp/grpcpp.h>
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "protos/perfetto/remote_writer/remote_writer.grpc.pb.h"
namespace perfetto {
class RemoteWriter {
 public:
  explicit RemoteWriter(std::string host, int port);
  ~RemoteWriter();
  bool WritePackets(const std::vector<TracePacket>& packets) {
    for (const TracePacket& packet : packets) {
      if (!WritePacket(packet)) {
        return false;
      }
    }
    return true;
  }
  bool WritePacket(const TracePacket& packet);
 private:
  std::string host_;
  int port_;
  std::shared_ptr<grpc::ChannelInterface> channel_;
  std::unique_ptr<protos::TraceDataService::Stub> stub_;
};
}  // namespace perfetto
#endif  // SRC_PERFETTO_CMD_REMOTE_WRITER_H_

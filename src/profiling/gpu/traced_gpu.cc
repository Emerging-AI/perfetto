
#include "src/profiling/gpu/traced_gpu.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/tracing/default_socket.h"
#include "src/profiling/gpu/gpu_producer.h"
#include "src/profiling/gpu/gpu_descriptors.h"

namespace perfetto {


int TracedGpuMain(int, char**) {
  base::UnixTaskRunner task_runner;

  DirectDescriptorGetter dev_fd_getter;

  profiling::ArmGpuProducer producer(&dev_fd_getter, &task_runner);
  // const char* env_notif = getenv("TRACED_GPU_NOTIFY_FD");
  // if (env_notif) {
  //   int notif_fd = atoi(env_notif);
  //   producer.SetAllDataSourcesRegisteredCb([notif_fd] {
  //     PERFETTO_CHECK(base::WriteAll(notif_fd, "1", 1) == 1);
  //     PERFETTO_CHECK(base::CloseFile(notif_fd) == 0);
  //   });
  // }
  producer.ConnectWithRetries(GetProducerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace perfetto

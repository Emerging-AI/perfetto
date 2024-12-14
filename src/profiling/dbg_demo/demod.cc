
#include "src/profiling/dbg_demo/demod.h"

#include <stdio.h>
#include <stdlib.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/event_fd.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/tracing/default_socket.h"
#include "src/profiling/dbg_demo/demod_producer.h"
#include "src/profiling/dbg_demo/wire_protocol.h"

#include "perfetto/ext/base/unix_task_runner.h"

namespace perfetto {
namespace profiling {
namespace {

int StartCentralDemod();


// int GetListeningSocket() {
//   const char* sock_fd = getenv(kDemodSocketEnvVar);
//   if (sock_fd == nullptr)
//     PERFETTO_FATAL("Did not inherit socket from init.");
//   char* end;
//   int raw_fd = static_cast<int>(strtol(sock_fd, &end, 10));
//   if (*end != '\0')
//     PERFETTO_FATAL("Invalid %s. Expected decimal integer.",
//                    kDemodSocketEnvVar);
//   return raw_fd;
// }

// 事件通知机制的fd，linux & android的eventfd / macos的pip /
// windows的CreateEvent
base::EventFd* g_dump_evt = nullptr;

int StartCentralDemod() {
  g_dump_evt = new base::EventFd();

  // producer相关配置和运行
  base::UnixTaskRunner task_runner;  // 多平台通用
  // base::Watchdog::GetInstance()->Start();  // crash on exceedingly long
  //   tasks
  DemodProducer producer(DemodMode::kCentral, &task_runner,
                         /* exit_when_done= */ false);

  // int listening_raw_socket = GetListeningSocket();
  // PERFETTO_LOG("listening_raw_socket = %d", listening_raw_socket);

  producer.ConnectWithRetries(GetProducerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace

int DemodMain(int argc, char** argv) {
  // cleanup_crash
  bool cleanup_crash = false;

  PERFETTO_LOG("%d\n", argc);
  PERFETTO_LOG("%s\n", argv[0]);

  if (cleanup_crash) {
    PERFETTO_LOG(
        "Recovering from crash: unsetting Demod system properties. "
        "Expect SELinux denials for unrelated properties.");
    // SystemProperties::ResetDemodProperties(); // TODO
    PERFETTO_LOG(
        "Finished unsetting Demod system properties. "
        "SELinux denials about properties are unexpected after "
        "this point.");
    return 0;
  }

  // start as a central daemon.
  return StartCentralDemod();
}

}  // namespace profiling
}  // namespace perfetto
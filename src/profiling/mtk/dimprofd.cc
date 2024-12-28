
#include <stdio.h>
#include <stdlib.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/getopt.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/version.h"
#include "perfetto/tracing/default_socket.h"

#include "src/profiling/mtk/dimprofd_producer.h"

namespace perfetto {
namespace profiling {
namespace {
void PrintUsage(const char* prog_name) {
  fprintf(stderr, R"(
Usage: %s [option] ...
Options and arguments
    --background : Exits immediately and continues running in the background
    --version : print the version number and exit.

Example:
    %s --background
    starts the service.
)",
          prog_name, prog_name);
}

}  // namespace

int DimprofdMain(int argc, char** argv) {
  enum LongOption {
    OPT_VERSION = 1000,
    OPT_BACKGROUND,
  };

  //   bool background = false;

  static const option long_options[] = {
      {"version", no_argument, nullptr, OPT_VERSION},
      //   {"background", no_argument, nullptr, OPT_BACKGROUND},
      {nullptr, 0, nullptr, 0}};

  for (;;) {
    int option = getopt_long(argc, argv, "", long_options, nullptr);
    if (option == -1)
      break;
    switch (option) {
        //   case OPT_BACKGROUND:
        //     background = true;
        //     break;
      case OPT_VERSION:
        printf("%s\n", base::GetVersionString());
        return 0;
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  // ? background: like android internal deamon/service
  // PERFETTO_LOG("background: %d", background);

  PERFETTO_LOG("Starting %s service", argv[0]);
  base::UnixTaskRunner task_runner;

  DimprofdProducer producer;

  producer.ConnectWithRetries(GetProducerSocket(), &task_runner);

  task_runner.Run();
  return 0;
}

}  // namespace profiling
}  // namespace perfetto
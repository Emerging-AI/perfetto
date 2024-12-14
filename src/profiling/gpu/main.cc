#include "src/profiling/gpu/traced_gpu.h"

int main(int argc, char** argv) {
  return perfetto::TracedGpuMain(argc, argv);
}

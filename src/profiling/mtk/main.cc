#include <iostream>


#include "src/profiling/mtk/dimprofd.h"

int main(int argc, char** argv) {
  return perfetto::profiling::DimprofdMain(argc, argv);
}


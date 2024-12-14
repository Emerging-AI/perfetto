#include <iostream>


#include "src/profiling/dbg_demo/demod.h"

int main(int argc, char** argv) {
  std::cout << "debug demo" << std::endl;

  return perfetto::profiling::DemodMain(argc, argv);
}

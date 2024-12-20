

tools/gn args out/traced_dbg_demo --export-compile-commands
tools/gn gen out/traced_dbg_demo --export-compile-commands

tools/gn ls out/traced_dbg_demo

tools/gn ls out/traced_dbg_demo > out/traced_dbg_demo/all_targets.txt

tools/ninja -C out/traced_dbg_demo -t targets > out/traced_dbg_demo/all_ninja_targets.txt


tools/ninja -C out/traced_dbg_demo -t graph  >  out/traced_dbg_demo/build.dot
dot -Tpng out/traced_dbg_demo/build.dot -o out/traced_dbg_demo/build.png

tools/ninja -C out/traced_dbg_demo -t graph traced_dbg_demo >  out/traced_dbg_demo/traced_dbg_demo.dot
dot -Tpng out/traced_dbg_demo/traced_dbg_demo.dot -o out/traced_dbg_demo/traced_dbg_demo.png


unflatten -f -l 4 -c 6 out/traced_dbg_demo/traced_dbg_demo.dot | dot | gvpack -array_t6 | neato -s -n2 -Tpng -o out/traced_dbg_demo/traced_dbg_demo.png



tools/ninja -C out/traced_dbg_demo traced_dbg_demo

tools/ninja -C out/traced_dbg_demo demod



tools/ninja -C out/traced_dbg_demo -t clean traced_dbg_demo 
tools/ninja -C out/traced_dbg_demo traced_dbg_demo 

tools/ninja -C out/traced_dbg_demo memory:unittests

tools/ninja -C out/traced_dbg_demo -t targets | grep unittests 
tools/ninja -C out/traced_dbg_demo src/profiling/memory:unittests


----
perfetto % tools/gn args --list out/traced_dbg_demo | grep -C 5 enable_perfetto_unittests 

enable_perfetto_ui
    Current value (from the default) = true
      From //gn/perfetto.gni:353

enable_perfetto_unittests
    Current value (from the default) = true
      From //gn/perfetto.gni:200

enable_perfetto_version_gen
    Current value (from the default) = true

------



tools/gn desc out/traced_dbg_demo traced_dbg_demo 


tools/gn desc out/traced_dbg_demo heapprofd_standalone_client_example

tools/ninja -C out/traced_dbg_demo heapprofd_standalone_client_example 

tools/ninja -C out/traced_dbg_demo -t graph heapprofd_standalone_client_example >  out/traced_dbg_demo/heapprofd_standalone_client_example.dot

dot -Tpng out/traced_dbg_demo/heapprofd_standalone_client_example.dot -o out/traced_dbg_demo/heapprofd_standalone_client_example.png


unflatten -f -l 4 -c 6 out/traced_dbg_demo/heapprofd_standalone_client_example.dot | dot | gvpack -array_t6 | neato -s -n2 -Tpng -o out/traced_dbg_demo/heapprofd_standalone_client_example.png


tools/ninja -C out/traced_dbg_demo demod && adb push ./out/traced_dbg_demo/demod /data/local/tmp && adb shell /data/local/tmp/demod


--------

CFG='buffers {
  size_kb: 63488
}

data_sources {
  config {
    name: "perfetto.demod"
  }
}

duration_ms: 0
write_into_file: true
flush_timeout_ms: 30000
flush_period_ms: 604800000

'; echo ${CFG} | perfetto --txt -c - -o /data/misc/perfetto-traces/profile-000000 -d


CFG='buffers {
  size_kb: 63488
}

data_sources {
  config {
    name: "linux.arm_gpu_counter"
  }
}

duration_ms: 0
write_into_file: true
flush_timeout_ms: 30000
flush_period_ms: 604800000

'; echo ${CFG} | perfetto --txt -c - -o /data/misc/perfetto-traces/profile-000000 -d
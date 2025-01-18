

tools/gn args out/traced_dbg_demo --export-compile-commands
tools/gn gen out/mac_debug --export-compile-commands
tools/gn gen out/traced_dbg_demo --export-compile-commands

tools/gn ls out/traced_dbg_demo

tools/gn ls out/traced_dbg_demo > out/traced_dbg_demo/all_targets.txt

tools/ninja -C out/traced_dbg_demo -t targets > out/traced_dbg_demo/all_ninja_targets.txt
tools/ninja -C out/mac_debug -t targets > out/mac_debug/all_ninja_targets.txt


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


tools/ninja -C out/traced_dbg_demo lib_gpu_counters_device_private

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

tools/ninja -C out/traced_dbg_demo perfetto && adb push ./out/traced_dbg_demo/perfetto /data/local/tmp && adb shell /data/local/tmp/demod


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

'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/misc/perfetto-traces/profile-000000 -d


tools/ninja -C out/traced_dbg_demo perfetto && adb push ./out/traced_dbg_demo/perfetto /data/local/tmp

CFG='buffers {
  size_kb: 63488
}

data_sources {
  config {
    name: "linux.arm_gpu_stats"
    arm_gpu_stats_config {
      gpuinfo_period_ms: 500
      arm_gpu_counters: MALI_GPU_ACTIVE_CY
      arm_gpu_counters: MALI_ANY_ACTIVE_CY
      arm_gpu_counters: MALI_GEOM_SAMPLE_CULL_RATE
    }
  }
}

duration_ms: 0
flush_timeout_ms: 100
flush_period_ms: 604800000

'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/local/tmp/freq-000003 -d




adb -s 0123456789ABCDEF push out/traced_dbg_demo/lib_gpu_counters_api_example /data/local/tmp

adb -s 0123456789ABCDEF shell /data/local/tmp/lib_gpu_counters_api_example


adb push out/traced_dbg_demo/lib_gpu_counters_api_example /data/local/tmp

adb shell /data/local/tmp/lib_gpu_counters_api_example



CFG='buffers {
  size_kb: 63488
}

data_sources: {
 config: {
  name: "linux.process_stats"
  process_stats_config: {
   scan_all_processes_on_start: true
   proc_stats_poll_ms: 1000
   scan_smaps_rollup: true
   record_process_runtime: true
  }
 }
}

duration_ms: 0
flush_timeout_ms: 30000
flush_period_ms: 604800000

'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/local/tmp/profile-00018 -d

/data/local/tmp/perfetto --txt -c - -o /data/local/tmp/profile-00016 -d

perfetto --txt -c - -o /data/misc/perfetto-traces/profile-00017 -d
/data/misc/perfetto-traces

--------

tools/ninja -C out/traced_dbg_demo perfetto && adb -s 17b96d97 push out/traced_dbg_demo/perfetto /data/local/tmp

adb push out/traced_dbg_demo/traced_probes /data/local/tmp/traced_probes_new


adb -s 0123456789ABCDEF pull /data/local/tmp/profile-00014 out
adb  pull /data/misc/perfetto-traces/profile-00017 out

adb -s 17b96d97 shell

adb -s 17b96d97 pull /data/local/tmp/profile-00015 out
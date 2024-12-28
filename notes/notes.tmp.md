

```bash

# 基于48.1进行实验
git checkout -b tmp/jocky v48.1
```


##
https://github.com/danbev/learning-v8/blob/master/notes/gn.md

```bash
#gn生成ninja.build
tools/gn gen out/mac_debug '--args=is_clang=true is_debug=true' --check

tools/ninja -C out/mac_debug tracebox traced traced_probes perfetto 

```


```bash
bazel query --noimplicit_deps \
"deps(//:traced_probes)" \
--notool_deps --output graph | dot -Tsvg > traced_probes.svg

bazel query --noimplicit_deps \
"deps(kind(cc_library, //:traced_probes))" \
--notool_deps --output build | grep '^cc_library'

bazel query --noimplicit_deps \
"kind("cc_library", //:traced_probes)" \
--notool_deps --output build 
```


## cpu_porfile

- 需要Android T+

edu.cs4730.opengl30cube


mkdir out/cpuf12
./tools/cpu_profile -n "xyz.jockyhawk.trianglegl" -d 10000 -o out/cpuf12 --print-config

./out/mac_debug/traceconv profile --pid 6734 out/cpuf2/profile.1.pid.15637.pb out/cpuf2/profile.1.pid.15637.json

6734

### 对应的cfg
```
buffers {
  size_kb: 2048
}

buffers {
  size_kb: 63488
}

data_sources {
  config {
    name: "linux.process_stats"
    target_buffer: 0
    process_stats_config {
      proc_stats_poll_ms: 100
    }
  }
}

duration_ms: 10000
write_into_file: true
flush_timeout_ms: 30000
flush_period_ms: 604800000

data_sources {
  config {
    name: "linux.perf"
    target_buffer: 1
    perf_event_config {
      timebase {
        counter: SW_CPU_CLOCK
        frequency: 100
        timestamp_clock: PERF_CLOCK_MONOTONIC
      }
      callstack_sampling {
        scope {
          target_cmdline: "edu.cs4730.opengl30cube"
        }
        kernel_frames: false
      }
    }
  }
}
```


## power

### atrace
./tools/record_android_trace -c test/configs/atrace_power.cfg -o atrace_power2.ptf 
./tools/record_android_trace -c test/configs/cpu_profile.cfg -o ./out/cpu12/cpu_profile.pb 

### ftrace


## gpu freq
./tools/record_android_trace -c test/configs/gpu_freq.cfg -o out/gpu_freq2.ptf 


./tools/record_android_trace -c test/configs/gpu_usage.cfg -o out/gpu_usage2.ptf 



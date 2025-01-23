CFG='buffers:  {
  size_kb:  20480
  fill_policy:  RING_BUFFER
}

data_sources:  {
  config:  {
    name:  "linux.arm_gpu_stats"
    target_buffer:  0
    arm_gpu_stats_config:  {
      gpuinfo_period_ms:  1000
      arm_gpu_counters:  MALI_LS_RD_CY
    }
  }
}

duration_ms:  15000
flush_period_ms:  5000
'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/local/tmp/agi-100004 -d


MALI_LS_ATOMIC
MALI_LS_RD_CY
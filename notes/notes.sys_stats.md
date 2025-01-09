CFG='buffers:  {
  size_kb:  2048
  fill_policy:  DISCARD
}

data_sources {
  config {
    name: "linux.sys_stats"
    sys_stats_config {
      meminfo_period_ms: 100
      meminfo_counters: MEMINFO_MEM_AVAILABLE
      meminfo_counters: MEMINFO_BUFFERS
      meminfo_counters: MEMINFO_CACHED
      meminfo_counters: MEMINFO_SWAP_CACHED
      meminfo_counters: MEMINFO_ACTIVE
      meminfo_counters: MEMINFO_INACTIVE
      meminfo_counters: MEMINFO_ACTIVE_ANON
      meminfo_counters: MEMINFO_INACTIVE_ANON
      meminfo_counters: MEMINFO_ACTIVE_FILE
      meminfo_counters: MEMINFO_INACTIVE_FILE
      meminfo_counters: MEMINFO_UNEVICTABLE

      vmstat_period_ms: 100
      vmstat_counters: VMSTAT_NR_FREE_PAGES
      vmstat_counters: VMSTAT_NR_ALLOC_BATCH
      vmstat_counters: VMSTAT_NR_INACTIVE_ANON
      vmstat_counters: VMSTAT_NR_ACTIVE_ANON
      vmstat_counters: VMSTAT_NR_INACTIVE_FILE
      vmstat_counters: VMSTAT_NR_ACTIVE_FILE
      vmstat_counters: VMSTAT_NR_UNEVICTABLE
      vmstat_counters: VMSTAT_NR_MLOCK
      vmstat_counters: VMSTAT_NR_ANON_PAGES
      vmstat_counters: VMSTAT_NR_MAPPED
      vmstat_counters: VMSTAT_NR_FILE_PAGES
      vmstat_counters: VMSTAT_NR_DIRTY
      vmstat_counters: VMSTAT_NR_WRITEBACK
      vmstat_counters: VMSTAT_NR_SLAB_RECLAIMABLE
      vmstat_counters: VMSTAT_NR_SLAB_UNRECLAIMABLE
      vmstat_counters: VMSTAT_NR_PAGE_TABLE_PAGES
      vmstat_counters: VMSTAT_NR_KERNEL_STACK
      vmstat_counters: VMSTAT_NR_OVERHEAD
      vmstat_counters: VMSTAT_NR_UNSTABLE
      vmstat_counters: VMSTAT_NR_BOUNCE
      vmstat_counters: VMSTAT_NR_VMSCAN_WRITE
      vmstat_counters: VMSTAT_NR_VMSCAN_IMMEDIATE_RECLAIM
      vmstat_counters: VMSTAT_NR_WRITEBACK_TEMP

      stat_period_ms: 100
      stat_counters: STAT_CPU_TIMES
      stat_counters: STAT_IRQ_COUNTS
      stat_counters: STAT_FORK_COUNT
    }
  }
}
duration_ms:  0
flush_period_ms : 500
'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/local/tmp/atrace-100024 -d


```shell
CFG='buffers {
  size_kb: 100024
  fill_policy: RING_BUFFER
}

data_sources {
  config {
    name: "linux.ftrace"
    target_buffer: 0
    ftrace_config {
      atrace_categories: "gfx"
      atrace_categories: "view"
      atrace_categories: "sync"
      atrace_apps: "xyz.jockyhawk.trianglegl"
      buffer_size_kb: 168
      drain_period_ms: 42
    }
  }
}

duration_ms: 0
'; echo ${CFG} | /data/local/tmp/perfetto --txt -c - -o /data/local/tmp/atrace-000000 -d

adb pull /data/local/tmp/atrace-000000 out
```


```sql
select frame_no_diff * 1000000000.0 / ts_diff as fps, a.frame_no, a.ts  from (

SELECT
    frame_no,
    ts,
    ts - LAG(ts) OVER (ORDER BY ts) AS ts_diff,
    frame_no - LAG(frame_no) OVER (ORDER BY ts) AS frame_no_diff
FROM (
SELECT
    SUBSTR(
        name,
        INSTR(name, 'frame=') + 6,
        LENGTH(name) - INSTR(name, 'frame=') - 5
    ) AS frame_no,
    __intrinsic_slice.*
FROM __intrinsic_slice where name like '%acquireNextBufferLocked%' )  ) a where a.ts_diff is not null;
```


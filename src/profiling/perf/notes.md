

tools/gn args out/traced_perf --export-compile-commands


tools/gn ls out/traced_perf

tools/gn ls out/traced_perf > out/traced_perf/all_targets.txt

tools/ninja -C out/traced_perf -t targets > out/traced_perf/all_ninja_targets.txt


tools/ninja -C out/traced_perf -t graph  >  out/traced_perf/build.dot
dot -Tpng out/traced_perf/build.dot -o out/traced_perf/build.png

tools/ninja -C out/traced_perf -t graph traced_perf >  out/traced_perf/traced_perf.dot
dot -Tpng out/traced_perf/traced_perf.dot -o out/traced_perf/traced_perf.png

sfdp -x -Goverlap=scale -Tsvg out/traced_perf/traced_perf.dot -o out/traced_perf/traced_perf.svg


tools/ninja -C out/traced_perf traced_perf



tools/ninja -C out/traced_perf -t clean traced_perf 
tools/ninja -C out/traced_perf traced_perf 



--------


adb push ./out/traced_dbg_demo/demod /data/local/tmp
adb shell /data/local/tmp/demod


tools/ninja -C out/traced_dbg_demo demod && adb push ./out/traced_dbg_demo/demod /data/local/tmp && adb shell /data/local/tmp/demod
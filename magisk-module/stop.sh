#!/system/bin/sh
# Stop only this tool's watcher; it gracefully releases its child HUD.
pid=$(cat /data/adb/gamma-perf/watch.pid 2>/dev/null)
case "$pid" in ''|*[!0-9]*) ;; *)
  if [ "$(cat /proc/$pid/comm 2>/dev/null)" = perf_watch ]; then
    kill -TERM "$pid" 2>/dev/null
    i=0
    while [ "$(cat /proc/$pid/comm 2>/dev/null)" = perf_watch ]; do
      i=$((i+1)); [ "$i" -lt 50 ] || return 1
      sleep 0.1
    done
  fi;;
esac
for pid in $(pidof perf_hud 2>/dev/null); do
  executable=$(readlink /proc/$pid/exe 2>/dev/null)
  case "$executable" in
    /data/adb/gamma-perf/perf_hud*|/data/local/tmp/perf_hud*|/data/adb/modules*/gamma_perf_hud/bin/perf_hud*)
      kill -TERM "$pid" 2>/dev/null
      i=0
      while [ "$(cat /proc/$pid/comm 2>/dev/null)" = perf_hud ]; do
        i=$((i+1)); [ "$i" -lt 50 ] || return 1
        sleep 0.1
      done;;
  esac
done
return 0

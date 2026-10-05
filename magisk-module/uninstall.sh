#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/stop.sh" || exit 1
rm -f /data/adb/service.d/99-gamma-perf-hud.sh
rm -rf /data/adb/gamma-perf
rm -f /data/local/tmp/gamma-perf-panel.sh /data/local/tmp/perf_hud /data/local/tmp/perf_watch

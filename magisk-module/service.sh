#!/system/bin/sh
MODDIR=${0%/*}
(
  while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 2; done
  [ ! -e "$MODDIR/disable" ] && [ ! -e "$MODDIR/remove" ] || exit 0
  # Retire the legacy startup hook so there is only one launcher.
  if [ -f /data/adb/service.d/99-gamma-perf-hud.sh ]; then
    mkdir -p /data/adb/gamma-perf
    cp /data/adb/service.d/99-gamma-perf-hud.sh /data/adb/gamma-perf/legacy-boot-backup.sh
    rm -f /data/adb/service.d/99-gamma-perf-hud.sh
  fi
  . "$MODDIR/stop.sh" || exit 1
  mkdir -p /data/adb/gamma-perf
  chmod 700 /data/adb/gamma-perf
  # Keep the existing ADB management commands pointed at this module version.
  for binary in perf_hud perf_watch; do
    cp "$MODDIR/bin/$binary" "/data/adb/gamma-perf/$binary.new" || exit 1
    chmod 700 "/data/adb/gamma-perf/$binary.new"
    mv "/data/adb/gamma-perf/$binary.new" "/data/adb/gamma-perf/$binary"
  done
  if [ ! -f /data/adb/gamma-perf/module-managed ]; then
    touch /data/adb/gamma-perf/enabled /data/adb/gamma-perf/module-managed
  fi
  rm -f /data/adb/gamma-perf/touch-probe
  [ -f /data/adb/gamma-perf/enabled ] || exit 0
  nohup "$MODDIR/bin/perf_watch" "$MODDIR/bin/perf_hud" </dev/null >/dev/null 2>&1 &
) &

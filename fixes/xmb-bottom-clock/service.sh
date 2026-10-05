#!/system/bin/sh
# Nano can start before Magisk mounts modules. Reload only at the idle main menu.
MODDIR=${0%/*}
EXPECTED=1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10
hash_file() { sha256sum "$1" 2>/dev/null | cut -d ' ' -f 1; }
i=0
while [ "$i" -lt 900 ]; do
  [ ! -e "$MODDIR/disable" ] && [ ! -e "$MODDIR/remove" ] || exit 0
  if [ "$(getprop sys.boot_completed)" = 1 ] &&
     [ "$(getprop sys.gammaos.nano.menu_active)" = 1 ] &&
     [ -z "$(getprop sys.gammaos.nano.launched_pkg)" ] &&
     ! pidof drastic-nano >/dev/null 2>&1; then
    [ "$(hash_file /system/bin/gammaos-nano)" = "$EXPECTED" ] || exit 1
    pids=$(pidof gammaos-nano)
    [ -n "$pids" ] || { i=$((i+1)); sleep 2; continue; }
    reload=0
    for pid in $pids; do
      [ "$(hash_file /proc/$pid/exe)" = "$EXPECTED" ] || reload=1
    done
    if [ "$reload" = 1 ]; then
      # Do not interfere with an independently running overlay service.
      [ "$(getprop init.svc.gammaos-nano-overlay)" != running ] || { i=$((i+1)); sleep 2; continue; }
      log -t GammaClockFix 'Reloading main menu after Magisk mount'
      setprop ctl.restart gammaos-nano
    fi
    exit 0
  fi
  i=$((i+1)); sleep 2
done
log -t GammaClockFix 'Main menu not idle; reload deferred until next boot'

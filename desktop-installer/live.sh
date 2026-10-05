#!/system/bin/sh
set -eu
STATE=/data/adb/gamma-clock-live
ORIGINAL=5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2
PATCHED=1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10
file_digest() { sha256sum "$1" 2>/dev/null | cut -d ' ' -f 1; }
idle() {
 [ "$(getprop sys.boot_completed)" = 1 ] &&
 [ "$(getprop sys.gammaos.nano.menu_active)" = 1 ] &&
 [ -z "$(getprop sys.gammaos.nano.launched_pkg)" ] &&
 [ "$(getprop init.svc.gammaos-nano-overlay)" != running ] &&
 ! pidof drastic-nano >/dev/null 2>&1
}
detach_clock_mounts() {
 i=0
 while grep -q ' /system/bin/gammaos-nano ' /proc/self/mountinfo; do
  i=$((i+1)); [ "$i" -le 10 ] || return 1
  umount -l /system/bin/gammaos-nano || return 1
 done
}
apply() {
 action=$1
 detach_clock_mounts
 [ "$(file_digest /system/bin/gammaos-nano)" = "$ORIGINAL" ] || exit 1
 if [ "$action" = clock-apply ]; then
  pending=/data/adb/modules_update/gamma_xmb_clock_fix
  mod=/data/adb/modules/gamma_xmb_clock_fix
  [ -f "$pending/module.prop" ] && [ ! -e "$pending/remove" ] && [ ! -e "$pending/disable" ] || exit 1
  [ "$(file_digest "$pending/system/bin/gammaos-nano")" = "$PATCHED" ] || exit 1
  # Preserve the previous tree until the new mount and process have been checked.
  [ ! -e /data/adb/gamma-clock-previous ] || { echo "旧模块文件仍被占用，请重启后重试。"; exit 1; }
  [ ! -d "$mod" ] || mv "$mod" "/data/adb/gamma-clock-previous"
  mv "$pending" "$mod"
  mount --bind "$mod/system/bin/gammaos-nano" /system/bin/gammaos-nano || {
   mv "$mod" "$pending"
   [ ! -d "/data/adb/gamma-clock-previous" ] || mv "/data/adb/gamma-clock-previous" "$mod"
   exit 1
  }
  expected=$PATCHED
 else
  [ "$(file_digest "$STATE/original")" = "$ORIGINAL" ] || exit 1
  expected=$ORIGINAL
 fi
 setprop ctl.restart gammaos-nano
 i=0
 while [ "$i" -lt 30 ]; do
  sleep 1; i=$((i+1))
  pids=$(pidof gammaos-nano 2>/dev/null || true)
  [ -n "$pids" ] || continue
  good=1
  for pid in $pids; do [ "$(file_digest /proc/$pid/exe)" = "$expected" ] || good=0; done
  [ "$good" = 1 ] && break
 done
 if [ "${good:-0}" != 1 ]; then
  # Recover a working original menu; keep installation disabled for a safe reboot.
  touch /data/adb/modules/gamma_xmb_clock_fix/disable 2>/dev/null || true
  mount --bind "$STATE/original" /system/bin/gammaos-nano
  setprop ctl.restart gammaos-nano
  echo '自动加载验证失败，已恢复原主菜单，请重启后重试。'; exit 1
 fi
 if [ "$action" = clock-remove ]; then
  rm -rf /data/adb/modules_update/gamma_xmb_clock_fix
  # Magisk can pin old module files until reboot; retain disabled cleanup markers.
  rm -rf /data/adb/modules/gamma_xmb_clock_fix 2>/dev/null || true
  if [ -d /data/adb/modules/gamma_xmb_clock_fix ]; then
    touch /data/adb/modules/gamma_xmb_clock_fix/disable /data/adb/modules/gamma_xmb_clock_fix/remove
  fi
  echo '时钟补丁已移除，原主菜单已自动恢复，无需重启。'
 else
  rm -rf /data/adb/gamma-clock-previous 2>/dev/null || true
  echo '时钟补丁已自动生效，运行程序校验通过，无需重启。'
 fi
}
if [ "${1:-}" = worker ]; then
 trap 'rm -rf "$STATE"' EXIT
 i=0
 while ! idle; do
  i=$((i+1)); [ "$i" -lt 1800 ] || { echo '等待主菜单超时，请重启掌机生效。'; exit 1; }
  sleep 2
 done
 apply "$2"
 exit 0
fi
mkdir "$STATE" 2>/dev/null || { echo '已有时钟自动操作等待完成，请退出游戏后再操作。'; exit 1; }
cp "$0" "$STATE/live.sh"
cp /data/local/tmp/gamma-clock-original "$STATE/original"
chmod 755 "$STATE/original"
chcon u:object_r:bootanim_exec:s0 "$STATE/original"
[ "$(file_digest "$STATE/original")" = "$ORIGINAL" ] || { rm -rf "$STATE"; exit 1; }
if idle; then
 sh "$STATE/live.sh" worker "$1"
else
 nohup sh "$STATE/live.sh" worker "$1" </dev/null >/data/local/tmp/gamma-clock-live.log 2>&1 &
 echo '已安排自动生效：退出游戏回到主菜单后自动加载，无需重启。'
fi

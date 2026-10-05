#!/system/bin/sh
set -eu
fail() { echo "失败：$*"; exit 1; }
[ "$(id -u)" = 0 ] || fail '未获得 Root，请在掌机上允许 Shell 超级用户权限。'
[ "$(getprop ro.product.cpu.abi)" = arm64-v8a ] || fail '仅支持 ARM64。'
[ -e /sys/class/devfreq/fde60000.gpu/cur_freq ] || fail 'GPU 不符合 RG DS 适配要求。'
grep -q gt9xx-0 /proc/bus/input/devices || fail '触摸设备不符合 RG DS 适配要求。'
[ -x /system/bin/drastic-nano ] || fail '需要 GammaOS 内置 Nano NDS。'
command -v magisk >/dev/null || fail '未检测到 Magisk；请先安装兼容的系统。'
echo "适配检查通过；Magisk $(magisk -v)"
clock_original=5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2
clock_patched=1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10
clock_mod=/data/adb/modules/gamma_xmb_clock_fix
clock_pending=/data/adb/modules_update/gamma_xmb_clock_fix
verify_clock() {
  if [ -d /data/adb/gamma-clock-live ]; then
    echo '时钟补丁：等待空闲主菜单自动生效。'; return
  fi
  if [ -d "$clock_pending" ]; then
    if [ -e "$clock_pending/remove" ] || [ -e "$clock_pending/disable" ]; then
      echo '时钟补丁：待移除，请重启掌机。'
    else
      echo '时钟补丁：待重启生效。'
    fi
    return
  fi
  if [ -d "$clock_mod" ]; then
    if [ -e "$clock_mod/remove" ] || [ -e "$clock_mod/disable" ]; then
      pids=$(pidof gammaos-nano 2>/dev/null || true)
      restored=1
      [ -n "$pids" ] || restored=0
      for pid in $pids; do
        [ "$(sha256sum /proc/$pid/exe | cut -d ' ' -f 1)" = "$clock_original" ] || restored=0
      done
      if [ "$restored" = 1 ]; then echo '时钟补丁：已恢复原主菜单，旧文件下次重启清理。'
      else echo '时钟补丁：已禁用，等待恢复或重启。'; fi
      return
    fi
    [ "$(sha256sum /system/bin/gammaos-nano | cut -d ' ' -f 1)" = "$clock_patched" ] || fail '时钟补丁尚未加载或版本不符，请重启；系统升级前请移除补丁。'
    pids=$(pidof gammaos-nano 2>/dev/null || true)
    [ -n "$pids" ] || fail '主菜单程序未运行。'
    for pid in $pids; do
      [ "$(sha256sum /proc/$pid/exe | cut -d ' ' -f 1)" = "$clock_patched" ] || fail '补丁文件已挂载，但运行的主菜单仍为旧版；请更新安装器并重装时钟补丁。'
    done
    echo '时钟补丁：已生效，运行程序已核验，表盘上下与屏幕边缘相切。' 
  else
    echo '时钟补丁：未安装。'
  fi
}
verify_removed() {
  for path in /data/adb/modules/gamma_perf_hud /data/adb/modules_update/gamma_perf_hud /data/adb/gamma-perf /data/adb/service.d/99-gamma-perf-hud.sh; do
    [ ! -e "$path" ] || fail "残留文件：$path"
  done
  for pid in $(pidof perf_watch perf_hud 2>/dev/null); do
    executable=$(readlink /proc/$pid/exe 2>/dev/null || true)
    case "$executable" in
      /data/adb/gamma-perf/*|/data/local/tmp/perf_watch*|/data/local/tmp/perf_hud*|/data/adb/modules/gamma_perf_hud/*) fail '面板进程仍在运行。';;
    esac
  done
  echo '面板结果：未安装；模块、启动文件及后台进程已清理。'
}
case "${1:-check}" in
check)
  current=$(sha256sum /system/bin/gammaos-nano 2>/dev/null | cut -d ' ' -f 1)
  case "$current" in "$clock_original"|"$clock_patched"|eb56f122f19dfb64c1af441b1ee609de2336a00334dc547fb8e9d4af1c5b7a4c) echo 'XMB 时钟补丁：当前程序版本兼容。';; *) echo 'XMB 时钟补丁：当前程序版本不兼容，不能安装。';; esac
  exit 0;;
clock-remove)
  [ ! -d /data/adb/gamma-clock-live ] || fail '已有时钟操作等待完成，请退出游戏后再操作。'
  for mod in "$clock_mod" "$clock_pending"; do
    if [ -d "$mod" ]; then touch "$mod/disable" "$mod/remove"; fi
  done
  sh /data/local/tmp/gamma-desktop-live.sh clock-remove || fail '自动恢复失败，请重启掌机；模块已禁用。'
  exit 0;;
clock-verify) verify_clock; exit 0;;
remove)
  # Disable both installed and pending versions before stopping, preventing a boot launch.
  for mod in /data/adb/modules/gamma_perf_hud /data/adb/modules_update/gamma_perf_hud; do
    if [ -d "$mod" ]; then touch "$mod/disable" "$mod/remove"; fi
  done
  rm -f /data/adb/service.d/99-gamma-perf-hud.sh
  # Fixed, bundled stop routine; never execute arbitrary scripts from an existing module.
  stop_panel() {
    # Stop only this tool's watcher; it gracefully releases its child HUD.
    pid=$(cat /data/adb/gamma-perf/watch.pid 2>/dev/null)
    case "$pid" in ''|*[!0-9]*) ;; *)
      executable=$(readlink /proc/$pid/exe 2>/dev/null)
      if [ "$(cat /proc/$pid/comm 2>/dev/null)" = perf_watch ] && {
        case "$executable" in
          /data/adb/gamma-perf/perf_watch|/data/local/tmp/perf_watch|/data/adb/modules/gamma_perf_hud/bin/perf_watch) true;;
          *) false;;
        esac
      }; then
        kill -TERM "$pid" 2>/dev/null
        i=0
        while [ "$(cat /proc/$pid/comm 2>/dev/null)" = perf_watch ]; do
          i=$((i+1)); [ "$i" -lt 50 ] || return 1
          sleep 0.1
        done
      fi;;
    esac
    for pid in $(pidof perf_hud perf_watch 2>/dev/null); do
      executable=$(readlink /proc/$pid/exe 2>/dev/null)
      case "$executable" in
        /data/adb/gamma-perf/perf_hud|/data/local/tmp/perf_hud|/data/adb/modules/gamma_perf_hud/bin/perf_hud|/data/adb/gamma-perf/perf_watch|/data/local/tmp/perf_watch|/data/adb/modules/gamma_perf_hud/bin/perf_watch)
          kill -TERM "$pid" 2>/dev/null
          i=0
          while kill -0 "$pid" 2>/dev/null; do
            i=$((i+1)); [ "$i" -lt 50 ] || return 1
            sleep 0.1
          done;;
      esac
    done
    return 0
  }
  stop_panel || fail '停止面板失败，模块已禁用；请重启掌机后再次移除。'
  rm -rf /data/adb/modules/gamma_perf_hud /data/adb/modules_update/gamma_perf_hud /data/adb/gamma-perf
  rm -f /data/local/tmp/gamma-perf-panel.sh /data/local/tmp/perf_hud /data/local/tmp/perf_watch /data/local/tmp/gamma-desktop-module.zip
  verify_removed
  echo '移除完成，可以重新安装。建议重启掌机。'; exit 0;;
verify)
  verify_clock
  if [ ! -e /data/adb/modules/gamma_perf_hud ] && [ ! -e /data/adb/modules_update/gamma_perf_hud ]; then
    verify_removed; exit 0
  fi
  [ "$(getprop sys.boot_completed)" = 1 ] || fail '系统尚未完成启动。'
  mod=/data/adb/modules/gamma_perf_hud
  [ -f "$mod/module.prop" ] || fail '模块未安装。'
  [ ! -e "$mod/disable" ] && [ ! -e "$mod/remove" ] || fail '模块已禁用或待删除。'
  [ ! -d /data/adb/modules_update/gamma_perf_hud ] || fail '安装仍待重启生效。'
  pid=$(cat /data/adb/gamma-perf/watch.pid 2>/dev/null) || fail '监听器未启动。'
  case "$pid" in ''|*[!0-9]*) fail '监听器 PID 无效。';; esac
  [ "$(readlink /proc/$pid/exe)" = "$mod/bin/perf_watch" ] || fail '监听器未从模块运行。'
  version=$(sed -n 's/^version=//p' "$mod/module.prop")
  echo "面板结果：版本 $version · 已启用 · 后台监听正常"
  if pidof drastic-nano >/dev/null; then
    if pidof perf_hud >/dev/null; then echo '面板结果：游戏运行中，面板进程正常；BTN_MODE 显示/隐藏。'
    else fail '游戏运行中，但面板进程尚未启动，请稍后重试。'; fi
  else echo '面板结果：当前无游戏，进入 Nano NDS 后自动启动。'; fi
  exit 0;;
install|clock-install) ;;
*) fail '未知操作。';;
esac
module_id=gamma_perf_hud
zip=/data/local/tmp/gamma-desktop-module.zip
if [ "$1" = clock-install ]; then
  [ ! -d /data/adb/gamma-clock-live ] || fail '已有时钟操作等待完成，请退出游戏后再操作。'
  module_id=gamma_xmb_clock_fix
  zip=/data/local/tmp/gamma-desktop-clock.zip
  current=$(sha256sum /system/bin/gammaos-nano | cut -d ' ' -f 1)
  case "$current" in "$clock_original"|"$clock_patched"|eb56f122f19dfb64c1af441b1ee609de2336a00334dc547fb8e9d4af1c5b7a4c) ;; *) fail '当前主菜单程序版本不兼容，已拒绝安装时钟补丁。';; esac
fi
[ "$(sha256sum "$zip" | cut -d ' ' -f 1)" = "${2:-}" ] || fail '安装包校验失败。'
# Repair missing installer helpers only; never replace the root backend or boot image.
mkdir -p /data/adb/magisk
for item in busybox util_functions.sh; do
  [ -s /data/adb/magisk/$item ] && continue
  apk=$(pm path com.topjohnwu.magisk | sed -n 's/^package://p' | head -n 1)
  [ -f "$apk" ] || fail '缺少 Magisk 安装辅助文件和管理 App。'
  case "$item" in busybox) entry=lib/arm64-v8a/libbusybox.so;; *) entry=assets/util_functions.sh;; esac
  temp=/data/adb/magisk/$item.gamma-new
  unzip -p "$apk" "$entry" > "$temp" || { rm -f "$temp"; fail '提取 Magisk 辅助文件失败。'; }
  [ -s "$temp" ] || fail 'Magisk 辅助文件为空。'
  chmod 755 "$temp"
  mv "$temp" /data/adb/magisk/$item
done
/data/adb/magisk/busybox true || fail 'Magisk BusyBox 不可用。'
magisk --install-module "$zip" || fail 'Magisk 模块安装失败。'
[ -s /data/adb/modules_update/$module_id/module.prop ] || fail '未找到待生效模块。'
rm -f "$zip"
if [ "$1" = clock-install ]; then
  sh /data/local/tmp/gamma-desktop-live.sh clock-apply || fail '自动加载失败，请重启掌机生效。'
  echo '系统升级前请移除时钟补丁。' 
else
  pending=/data/adb/modules_update/gamma_perf_hud
  mod=/data/adb/modules/gamma_perf_hud
  # Use the stop routine from the just-verified package, not an old module.
  sh "$pending/stop.sh" || fail '停止旧面板失败，请重启掌机后重试。'
  previous=/data/adb/gamma-perf-previous
  [ ! -e "$previous" ] || fail '存在上次更新备份，请重启后重试。'
  [ ! -d "$mod" ] || mv "$mod" "$previous"
  mv "$pending" "$mod"
  sh "$mod/service.sh"
  i=0
  while [ "$i" -lt 30 ]; do
    sleep 1; i=$((i+1))
    pid=$(cat /data/adb/gamma-perf/watch.pid 2>/dev/null || true)
    case "$pid" in ''|*[!0-9]*) continue;; esac
    [ "$(readlink /proc/$pid/exe 2>/dev/null)" = "$mod/bin/perf_watch" ] && break
  done
  [ "$i" -lt 30 ] || fail '面板自动启动失败，请重启掌机后重试。'
  rm -rf "$previous"
  echo '性能面板已自动生效，无需重启；进入游戏后按 BTN_MODE 显示。' 
fi

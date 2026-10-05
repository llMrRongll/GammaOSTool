[ "$BOOTMODE" = true ] || abort "请在 Magisk App 中安装，不支持 Recovery 安装。"
[ "$ARCH" = arm64 ] || abort "仅支持 ARM64 RG DS。"
[ -e /sys/class/devfreq/fde60000.gpu/cur_freq ] || abort "未检测到适配的 RG DS GPU。"
grep -q 'gt9xx-0' /proc/bus/input/devices || abort "未检测到适配的 RG DS 触摸设备。"
[ -x /system/bin/drastic-nano ] || abort "需要 GammaOS 内置 Nano NDS。"
ui_print "安装 GammaOS RG DS 性能面板"
ui_print "进入游戏默认隐藏，按 BTN_MODE 显示/隐藏。"
ui_print "安装完成后请重启。"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/bin/perf_hud" 0 0 0755
set_perm "$MODPATH/bin/perf_watch" 0 0 0755
for script in service.sh uninstall.sh stop.sh; do set_perm "$MODPATH/$script" 0 0 0755; done

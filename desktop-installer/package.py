from pathlib import Path
import hashlib, shutil, subprocess, plistlib, zipfile
root = Path(__file__).resolve().parent
repo = root.parent
out = repo / 'dist' / 'desktop-installer'
out.mkdir(parents=True, exist_ok=True)
module = repo / 'dist/GammaOS-RGDS-Performance-HUD-v1.0.12.zip'
original_binary = repo/'fixes/xmb-bottom-clock/gammaos-nano.original'
assert hashlib.sha256(original_binary.read_bytes()).hexdigest() == '5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2', 'Original menu version mismatch'
clock_binary = repo / 'fixes/xmb-bottom-clock/gammaos-nano.patched'
assert hashlib.sha256(clock_binary.read_bytes()).hexdigest() == '1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10', 'Clock patch version mismatch'
clock_module = repo / 'dist/GammaOS-XMB-Clock-Fix-v1.0.2.zip'
with zipfile.ZipFile(clock_module, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('module.prop', 'id=gamma_xmb_clock_fix\nname=GammaOS XMB Bottom Clock Fit\nversion=1.0.2\nversionCode=3\nauthor=Local patch\ndescription=Fit bottom XMB clock tangent to panel edges. Remove before system updates.\n')
    info = zipfile.ZipInfo('system/bin/gammaos-nano'); info.external_attr = 0o100755 << 16; info.compress_type = zipfile.ZIP_DEFLATED
    z.writestr(info, clock_binary.read_bytes())
    z.write(repo/'fixes/xmb-bottom-clock/service.sh', 'service.sh')
    for name in ['LICENSE','NOTICE','DISCLAIMER.md','THIRD_PARTY_NOTICES.md']:
        z.write(repo/name,name)
    for path in sorted((repo/'licenses').glob('*.txt')):
        z.write(path,'licenses/'+path.name)
    z.writestr('customize.sh', 'SKIPUNZIP=0\ncurrent=$(sha256sum /system/bin/gammaos-nano | cut -d " " -f 1)\ncase "$current" in 5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2|1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10|eb56f122f19dfb64c1af441b1ee609de2336a00334dc547fb8e9d4af1c5b7a4c) ;; *) abort "Unsupported gammaos-nano version";; esac\nset_perm "$MODPATH/system/bin/gammaos-nano" 0 2000 0755 u:object_r:bootanim_exec:s0\n')
clock_digest = hashlib.sha256(clock_module.read_bytes()).hexdigest()
digest = hashlib.sha256(module.read_bytes()).hexdigest()
readme = '''GammaOS RG DS 性能面板安装器 1.0.21

内置性能面板 1.0.12、时钟补丁 1.0.2。
适用：RG DS / GammaOS Core 1.4.1 / 1.4.4 内置 Nano NDS；设备须已有 Root 和 Magisk。
不会解锁、刷系统、安装 Root，也不适用于其他掌机。

1. 完整解压到电脑。Windows 双击“打开安装器.cmd”；Mac 打开“Gamma 面板安装器.app”。
2. 掌机开启开发者选项中的 USB 调试，接数据线，解锁并允许电脑调试授权。
3. 点击“检测设备”，然后“一键安装”。如有超级用户授权弹窗，请允许 Shell。
4. 安装后自动加载并验证，无需重启；“重启并验证”保留为备用。
5. 进入内置 Nano NDS 游戏，按 BTN_MODE 显示/隐藏下屏全屏面板。默认关闭。

XMB 时钟修复：点击“安装时钟补丁”，自动生效并验证；正在游戏时退出到主菜单后自动加载。表盘上下与屏幕边缘相切。可单独检查或移除；移除后自动恢复原主菜单。仅支持已核验的主菜单程序版本，不兼容时拒绝安装；系统升级前务必先移除时钟补丁。与性能面板独立安装、独立移除。\n\nWindows：仅在一台 Windows 10 电脑测试，Windows 11 未实机验证。无需 Python、Android Studio 或手动敲命令。如检测不到设备，使用掌机厂家提供的 ADB 驱动并换数据线。
Mac：仅在作者使用的 M1 Mac mini 测试；Intel 版本仅编译通过。构建最低目标 macOS 12，未全面验证其他系统版本。本地签名版本未做 Developer ID 公证，下载分发后可能需要在“系统设置 → 隐私与安全性”允许打开。仅对来源可信的包操作。
安装失败可直接重试，日志留在窗口中。请勿同时使用多个安装器或连接多个 Android 设备。
等待开机超时不代表安装失败：确认掌机开机后点击“验证状态”。
卸载：连接掌机，点击“移除面板”。立即停止面板和后台监听，清理本工具的模块、待生效升级、旧启动脚本和运行数据；无需重启。不会卸载 Magisk 或其他模块。

测试仅覆盖上述两台电脑，不承诺其他电脑或系统兼容。本次 1.4.4 兼容更新在 Mac 重新编译检查，未在 Windows 再次实机复测。1.4.4 已实机验证面板启动、显示与 FPS；用户已在 1.4.1 实机使用新版安装工具测试，确认面板可正常打开。
ADB 来自 Google 官方 Platform-Tools，随包保留 NOTICE 和版本信息。
源码：https://android.googlesource.com/platform/packages/modules/adb/
'''

readme = (repo/'DISCLAIMER.md').read_text() + '\n\n' + readme

def common(target):
    for name in ['DISCLAIMER.md','LICENSE','NOTICE','THIRD_PARTY_NOTICES.md']:
        shutil.copy2(repo/name,target/name)
    shutil.copytree(repo/'licenses',target/'licenses',dirs_exist_ok=True)
    shutil.copy2(root/'live.sh', target/'live.sh')
    shutil.copy2(repo/'fixes/xmb-bottom-clock/gammaos-nano.original', target/'clock-original')
    shutil.copy2(clock_module, target/'clock.zip')
    (target/'clock.sha256').write_text(clock_digest+'\n')
    shutil.copy2(root/'device.sh', target/'device.sh')
    shutil.copy2(module,target/'module.zip')
    (target/'module.sha256').write_text(digest+'\n')

win = out / 'Gamma-HUD-Windows'
if win.exists(): shutil.rmtree(win)
win.mkdir(); common(win)
shutil.copy2(root/'Installer.ps1',win/'Installer.ps1')
(win/'打开安装器.cmd').write_bytes(b'@echo off\r\ncd /d "%~dp0"\r\nstart "" powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "%~dp0Installer.ps1"\r\n')
for name in ['adb.exe','AdbWinApi.dll','AdbWinUsbApi.dll','libwinpthread-1.dll','NOTICE.txt','source.properties']:
    shutil.copy2(root/'vendor/windows/platform-tools'/name, win/name)
(win/'使用说明.txt').write_text(readme,encoding='utf-8-sig')
mac = out/'Gamma-HUD-macOS'; mac.mkdir(exist_ok=True)
app = mac/'Gamma 面板安装器.app'
if app.exists(): shutil.rmtree(app)
contents=app/'Contents'; exe=contents/'MacOS'; res=contents/'Resources'
exe.mkdir(parents=True);res.mkdir();common(res)
subprocess.run(['lipo','-create','/tmp/gamma-installer-arm','/tmp/gamma-installer-intel','-output',str(exe/'GammaInstaller')],check=True)
with (contents/'Info.plist').open('wb') as f:
    plistlib.dump(dict(CFBundleExecutable='GammaInstaller',CFBundleIdentifier='cn.rongjun.gamma-hud-installer',CFBundleName='Gamma 面板安装器',CFBundlePackageType='APPL',CFBundleShortVersionString='1.0.21',LSMinimumSystemVersion='12.0',NSHighResolutionCapable=True),f)
for name in ['adb','NOTICE.txt','source.properties']:
    shutil.copy2(root/'vendor/mac/platform-tools'/name,res/name)
subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
(mac/'使用说明.txt').write_text(readme)
for folder in [win,mac]:
    dest=out/(folder.name+'-v1.0.21.zip')
    if folder == mac:
        subprocess.run(['ditto','-c','-k','--sequesterRsrc','--keepParent',str(folder),str(dest)],check=True)
    else:
        with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
            for p in sorted(folder.rglob('*')):
                if p.is_file():z.write(p,p.relative_to(out))
    archive_digest = hashlib.sha256(dest.read_bytes()).hexdigest()
    dest.with_suffix('.zip.sha256').write_text(archive_digest+'  '+dest.name+'\n')
    print(dest)

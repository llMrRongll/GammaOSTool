# 源码构建

仅构建不代表在其他设备验证通过；适用范围和免责声明见仓库首页。

## 性能面板

需要 Zig（支持 aarch64-linux-musl）、Linux DRM UAPI 头文件、Python 3。预览需要 clang；重新生成字形需要 Pillow 和本地 Noto Sans CJK 字体。已有 hud_cjk.h 可直接编译。

```sh
zig cc -target aarch64-linux-musl -static -O2 -Wall -Wextra -I /path/to/libdrm/include/drm perf_hud.c -lm -o perf_hud
zig cc -target aarch64-linux-musl -static -O2 -Wall -Wextra perf_watch.c -o perf_watch
python3 package_magisk.py
clang preview_hud.c -lm -o preview_hud
./preview_hud
```

preview_hud 输出 /tmp/gamma-hud-preview.ppm。正式构建不要定义 HUD_TOUCH_TEST 或 HUD_REGION_TEST；这些实验不包含在公开安装包中。

## 桌面安装器

在 Mac 安装 Xcode Command Line Tools。从 Google 官方下载 macOS/Windows Platform-Tools 到 desktop-installer/vendor/mac/platform-tools 和 vendor/windows/platform-tools，保留 NOTICE.txt、source.properties。vendor 不提交仓库。

时钟补丁所需 gammaos-nano.original 必须来自已授权的兼容固件，放在 fixes/xmb-bottom-clock，运行 patch_binary.py 生成 .patched。脚本、安装器均检查固定版本哈希，不能处理任意固件；这些固件二进制不放入源码仓库。

```sh
python3 fixes/xmb-bottom-clock/patch_binary.py
xcrun swiftc -swift-version 5 -target arm64-apple-macosx12.0 desktop-installer/Installer.swift -o /tmp/gamma-installer-arm
xcrun swiftc -swift-version 5 -target x86_64-apple-macosx12.0 desktop-installer/Installer.swift -o /tmp/gamma-installer-intel
python3 desktop-installer/package.py
```

输出在 dist/desktop-installer，Mac 使用 ad-hoc 签名，未做 Developer ID 公证。二进制发行由 Releases 承载，不提交 Git。

## 目录

- perf_hud.c / hud_ui.h / hud_fps.h：DRM 面板、共用绘制及下屏呈现 FPS。
- perf_watch.c / magisk-module：游戏生命周期与模块安装/卸载。
- desktop-installer：Swift/AppKit、PowerShell/WinForms、设备操作及打包脚本。
- fixes/xmb-bottom-clock：版本限定时钟修复与恢复。
- research：未纳入正式构建的触摸实验源代码/策略测试及 FPS 解析测试。
- previews：共用绘制代码生成的示例数据图像。
- licenses：第三方许可与版权通知。

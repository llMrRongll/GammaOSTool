# 第三方组件与许可

本项目原创源码采用 Apache License 2.0；第三方组件仍适用各自许可，不因本项目许可而改变。

- Noto Sans CJK 固定字形子集（hud_cjk.h）：SIL Open Font License 1.1，见 licenses/NotoSansCJK-NOTICE.txt 和 licenses/NotoSansCJK-COPYRIGHT.txt。
- 安装器附带 Google Android Platform-Tools/ADB：保留原始 NOTICE.txt 和 source.properties，第三方许可见安装器目录内文件。工具来源：https://developer.android.com/tools/releases/platform-tools 。
- XMB 时钟补丁基于 GammaOS gammaos-nano。原程序和修改后程序随安装器用于校验及恢复/安装；只修改下屏时钟缩放指令和专用常量，不改游戏模拟器。GammaOS 主程序版权为 Copyright (C) 2026 GammaOS，原 Android/GammaOS 通知见 licenses/GammaOS-AOSP-NOTICE.txt，Apache License 2.0 见 LICENSE；rcheevos 的 MIT 许可见 licenses/rcheevos-LICENSE.txt。上游源码：https://github.com/GammaOS/GammaOSNextDistribution-14 ，相关目录 frameworks/base/cmds/gammaos-nano。本项目补丁源码和二进制修改脚本位于 fixes/xmb-bottom-clock。

本项目不包含 ROM、BIOS、游戏存档或 DraStic 模拟器。GammaOS、Anbernic、Magisk 等名称用于说明适配对象，商标归各自权利人所有。

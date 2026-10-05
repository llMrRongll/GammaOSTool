# 下屏时钟修复

仅适用已核验的GammaOS Core1.4.1 gammaos-nano程序；先阅读仓库免责声明和安装说明。源码补丁与二进制脚本只改变下屏时钟缩放，使表盘上下相切。patch_binary.py校验原程序哈希，版本不匹配拒绝处理。原/修改后的程序不提交源码仓库。

上游： https://github.com/GammaOS/GammaOSNextDistribution-14 ，frameworks/base/cmds/gammaos-nano/NanoMenuPS3Clock.cpp。

系统更新前先移除补丁。复原通过安装器移除补丁或在Magisk禁用/移除模块后重启。

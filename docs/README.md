# 文档导航

本仓库的软件文档对应固件版本 **2.0.0**。当前版本号见根目录 [`VERSION`](../VERSION) 和 [`src/Version.h`](../src/Version.h)；固件环境与编译参数以 [`platformio.ini`](../platformio.ini)、`src/Config.h` 和 `src/PowerManager.cpp` 为准。

## 当前使用说明

- [技术文档](技术文档.md)：渲染、动画、触摸、电源状态和构建说明。
- [电池供电诊断](电池供电诊断.md)：硬件电源路径的已知信息、软件关机边界和电池测量限制。
- [验证记录](verification/README.md)：历史短时性能与电源状态测试，说明临时构建环境和结果适用范围。
- [版本变更记录](../CHANGELOG.md)：2.0.0 版本的主要变化。

## 硬件参考

- [ESP32-WROOM-32 引脚参考手册](ESP32-WROOM-32_引脚参考手册.md)
- [ESP32-WROOM-32 引脚功能图解](ESP32-WROOM-32_引脚功能图解.md)
- [STEP / STL 外壳与开发板说明书](../case/)

硬件参考针对当前仓库附带的 HW-950 / ESP32-2432S024R 板型，不应直接套用于外观相似但电源、电路不同的 CYD 板。

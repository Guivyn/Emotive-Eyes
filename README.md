# ESP32 Emotive Eyes

> 当前版本：1.2.1 · 当前配置/验证板型：HW-950 / ESP32-2432S024R

[![PlatformIO CI](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml/badge.svg)](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml)

<p align="center">
  <img src="./assets/readme/hero-1.2.1.svg" width="100%" alt="ESP32 Emotive Eyes 项目概览：过程式表情眼睛、触摸交互与分级电源管理">
</p>

面向 HW-950 / ESP32-2432S024R（Sunton CYD 系列）的过程式表情眼睛。所有面部几何都由 CPU 实时计算，不依赖任何预渲染图片资源；配合 ST7796 触摸屏，可以作为桌面 AI 宠物或互动小设备的“脸”。固件使用逻辑分辨率 320×240（TFT_eSPI 原始配置为 240×320，并固定旋转为横屏），仓库同时提供匹配板型的 STEP/STL 外壳文件。

## 运行行为与示例日志

开机直接显示闭眼画面，约 500ms 后睁眼醒来；约 5 秒后开始随机视线移动，约 8 秒后开启随机眨眼和表情切换。触摸屏幕可以立刻互动。

> 下方是 `esp32dev_debug` 环境的示例日志。发布环境默认关闭运行日志；需要观察手势和状态时，请使用后文的调试环境。

```text
=== ESP32 TFT Eyes — Pure Procedural ===
[OK] TFT ready (SPI=80MHz)
[OK] Touch ready (XPT2046 CS=33, shared SPI, threshold=350)
[TOUCH] Tap #1
[TOUCH] Tap #2
[TOUCH] Tap #3
[TOUCH] Angry face triggered by taps
[STATS] state=ACTIVE bl=100 interval=25ms temp=42.0C
```

## 特性

### 交互与表情

- 19 种表情预设：Normal、Angry、Glee、Happy、Sad、Worried、Focused、Annoyed、Surprised、Skeptic、Frustrated、Unimpressed、Sleepy、Suspicious、Squint、Furious、Scared、Awe、Coquette
- 随机眨眼、视线跟随与权重化表情切换，全部基于 `millis()` 时间基准，帧率无关
- XPT2046 触摸：单击反馈、双击复位、三连击触发 Angry、长按复位、上滑撒娇、下滑睡觉
- BOOT 长按 2 秒进入深睡；按 BOOT 或 EN 唤醒

### 固件能力

- 4 级电源状态机：ACTIVE → RELAX → IDLE → SLEEP，背光平滑渐变
- 芯片温度保护：≥70°C 强制降入 IDLE，≥80°C 强制进入 SLEEP
- 事件驱动渲染：画面静止时停止 SPI 传输，降低待机功耗
- 触摸校准可运行时调整：串口 `cal` 查看，`cal <offset> <range>` 设置并持久化
- LittleFS 默认不会自动格式化；维护时可显式发送 `fs-init`，仅在挂载失败时格式化

## 快速开始

需要先安装 [PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI），并确认接线与当前板型一致。

```bash
pio run -e esp32dev
pio device list
pio run -e esp32dev -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
pio test -e native
```

将 `YOUR_PORT` 替换为实际串口。需要运行日志时，使用 `esp32dev_debug` 环境：

```bash
pio run -e esp32dev_debug -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
```

`TFT_eSPI` 已通过 `lib_deps` 自动下载；屏幕引脚集中在 [`src/board_config.h`](src/board_config.h)，构建开关在 [`platformio.ini`](platformio.ini) 中，无需修改 `User_Setup.h`。

## 硬件接线

| 组件 | 型号 |
|------|------|
| MCU | ESP32-WROOM-32（当前配置：HW-950 / ESP32-2432S024R） |
| 屏幕 | ST7796 SPI TFT，320×240 |
| 触摸 | XPT2046，与 TFT 共用 SPI 总线 |
| 背光 | GPIO27（低电平关，高电平开） |
| RGB LED | R=GPIO17 / G=GPIO4 / B=GPIO16（低电平点亮） |
| 光敏 | GPIO34 |
| 电池 | 外接电池仅作供电，固件不读取电量/充电状态 |

| 信号 | GPIO |
|------|------|
| TFT_SCLK / TOUCH_CLK | GPIO14 |
| TFT_MOSI / TOUCH_MOSI | GPIO13 |
| TFT_MISO / TOUCH_MISO | GPIO12 |
| TFT_CS | GPIO15 |
| TFT_DC | GPIO2 |
| TFT_RST | 不接（-1，屏幕复位接板级 RESET） |
| TFT_BL | GPIO27 |
| TOUCH_CS | GPIO33 |
| BOOT（电源键） | GPIO0，长按 2 秒进入深睡 |
| I2C | SDA=GPIO21 / SCL=GPIO22 |

SPI 频率为 80MHz，RGB 顺序为 `TFT_BGR`。当前固件使用 TFT 与 XPT2046 共用的 SPI 信号线，触摸仅单独使用 `TOUCH_CS`。

## 3D 打印外壳

最新的外壳 CAD 与打印文件放在 [`case/`](case/)。STEP 适合继续编辑，STL 可直接导入切片软件：

| 部件 | CAD 源文件 | 打印文件 |
|------|------------|----------|
| 外壳主体 | [`cydcase（final）.step`](case/cydcase（final）.step) | [`cydcase（fianl）.stl`](case/cydcase（fianl）.stl) |
| 屏幕面板 / 前框 | [`CYDcaseDisplay(final).step`](case/CYDcaseDisplay%28final%29.step) | [`CYDcaseDisplay(final).stl`](case/CYDcaseDisplay%28final%29.stl) |

另附：[ESP32 开发板说明书](case/esp32开发板说明书.doc)。建议 PLA、0.2mm 层高打印，打印前按打印机尺寸检查缩放。

## 渲染管线

每一帧按顺序经过 5 级变换，输出链式传递：

```text
Transition → Transformation → Variation1 → Variation2 → Blink → FinalConfig → EyeDrawer::Draw()
```

- `Transition`：表情切换，500ms 从 Snapshot 插值到目标配置
- `Transformation`：视线方向与透视缩放，200ms 平滑过渡
- `Variation`：呼吸、脉冲等微表情
- `Blink`：眨眼，100ms → 50ms → 100ms 的梯形波形
- `EyeDrawer`：浮点边界 + 扫描线填充，优先写入 320×240 的 TFT_eSprite；若当前 ESP32 无法提供约 150KB 连续内存，则自动复用一个 160×240 全高分块推送左右两半，仍覆盖整屏

## 电源管理

| 状态 | 进入条件（距上次交互） | 背光 | 刷新率 | 表情行为 |
|------|------------------------|------|--------|----------|
| ACTIVE | 0–60 秒 | 100% | 25ms（40fps） | 完整动画 + 随机表情 |
| RELAX | 60 秒 | 50% | 33ms（30fps） | 固定表情、慢速视线、偶发哈欠 |
| IDLE | 3 分钟 | 25% | 41ms（约 24fps） | 固定 Normal、视线回中、偶发眨眼 |
| SLEEP | 5 分钟 | 0% | 不渲染，50ms 触摸轮询 | Sleepy 闭眼静态画面 |

任意触摸都会回到 ACTIVE 并重置空闲计时。背光使用 Gamma 2.2 感知曲线，状态切换时以缓动曲线平滑渐变（初始快、接近目标慢），避免亮度跳变。
CPU 频率随状态调整：ACTIVE 240MHz、RELAX 160MHz、IDLE 80MHz、SLEEP 80MHz；进入 SLEEP 时发送 `DISPOFF (0x28)`，唤醒时发送 `DISPON (0x29)` 降低面板功耗。

## 触摸手势

| 手势 | 动作 |
|------|------|
| 单击 | 快速眨眼作为反馈，重置空闲计时 |
| 双击（300ms 内） | 回到 Normal 表情 |
| 三连击（相邻点击间隔不超过 300ms） | 触发 Angry 表情 |
| 长按（2s） | 回到 Normal 表情 |
| 上滑 | 撒娇（Coquette） |
| 下滑 | 进入睡眠模式（Sleepy + Zzz），约 2 秒后切到低功耗 IDLE，再次触摸唤醒 |

按压力度阈值默认 `TOUCH_PRESSURE_THRESHOLD = 350`，不同面板可能需要微调。

## 构建开关

以下开关都定义在 `platformio.ini`：

| 开关 | 默认 | 说明 |
|------|------|------|
| `POWER_LIGHT_SLEEP_ENABLED` | 0 | GPIO36 补 10kΩ 上拉后改为 1，启用 LightSleep |
| `POWER_TEMP_PROTECT_ENABLED` | 1 | 芯片温度保护 |
| `SERIAL_DEBUG_ENABLED` | 0 | 串口调试日志；调试时改为 1 |

## 文档与验证

- [技术文档](docs/技术文档.md)：渲染管线、动画、电源管理与校准实现
- [引脚参考手册](docs/ESP32-WROOM-32_引脚参考手册.md)：当前板型的外设映射与硬件注意事项
- [引脚功能图解](docs/ESP32-WROOM-32_引脚功能图解.md)：板载外设和接口示意
- [PlatformIO CI](.github/workflows/ci.yml)：固件构建与主机侧测试

本地验证命令就是快速开始中的 `pio test -e native` 和 `pio run -e esp32dev`；硬件烧录/串口验证应使用 `esp32dev_debug`，屏幕显示质量仍需以实际屏幕观察为准。

## 项目结构

```text
ESP32-demo/
├── .github/workflows/
│   └── ci.yml
├── platformio.ini
├── LICENSE
├── README.md
├── assets/readme/
│   └── hero-1.2.1.svg
├── case/
│   ├── cydcase（final）.step
│   ├── cydcase（fianl）.stl
│   ├── CYDcaseDisplay(final).step
│   ├── CYDcaseDisplay(final).stl
│   └── esp32开发板说明书.doc
├── docs/
│   ├── 技术文档.md
│   ├── ESP32-WROOM-32_引脚参考手册.md
│   └── ESP32-WROOM-32_引脚功能图解.md
├── test/
│   ├── mocks/
│   └── test_native/
└── src/
    ├── main.cpp
    ├── Config.h
    ├── CalibrationCodec.h
    ├── board_config.h
    ├── Log.h
    ├── Animation.h
    ├── Eye.h
    ├── Eye.cpp
    ├── Face.h
    ├── Face.cpp
    ├── FacePower.h
    ├── FacePower.cpp
    ├── PowerManager.h
    └── PowerManager.cpp
```

## 已知限制

- LightSleep 需在 GPIO36 补 10kΩ 上拉并开启构建开关
- 光敏（GPIO34）未启用
- 触摸阈值和 SPI 频率可能因具体面板需要调整

## 许可

本项目基于 playfultechnology 的 ESP32 OLED Emotive Eyes 移植改造，原始项目由
Alastair Aitchison 发布，遵循 MIT 许可。本仓库以 GPL-3.0 发布，完整条款见
[`LICENSE`](LICENSE)。

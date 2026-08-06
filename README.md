# ESP32 Emotive Eyes

> 当前版本：1.1.1

<p align="center">
  <img src="./assets/readme/hero.svg?v=2" width="100%" alt="ESP32 Emotive Eyes 项目概览：过程式表情眼睛、触摸交互与分级电源管理">
</p>

ESP32 + ST7796 TFT（320×240）上的过程式表情眼睛。所有面部几何都由 CPU 实时计算，不依赖任何预渲染图片资源，适合做成桌面 AI 宠物或互动小设备的“脸”。

## 运行效果

开机先闭眼，2 秒后睁眼醒过来；之后开始随机眨眼、视线移动和表情切换。触摸屏幕可以立刻互动，串口会输出每一次手势和运行状态：

```text
=== ESP32 TFT Eyes — Pure Procedural ===
[OK] TFT ready (SPI=60MHz)
[OK] Touch ready (XPT2046 CS=33, shared SPI, threshold=350)
[TOUCH] Tap #1
[TOUCH] Tap #2
[TOUCH] Tap #3
[TOUCH] Angry face triggered by taps
[STATS] state=ACTIVE bl=100 interval=25ms temp=42.0C
```

## 特性

- 19 种表情预设：Normal、Angry、Glee、Happy、Sad、Worried、Focused、Annoyed、Surprised、Skeptic、Frustrated、Unimpressed、Sleepy、Suspicious、Squint、Furious、Scared、Awe、Coquette
- 随机眨眼、视线跟随与权重化表情切换，全部基于 `millis()` 时间基准，帧率无关
- 4 级电源状态机：ACTIVE → RELAX → IDLE → SLEEP，背光平滑渐变
- XPT2046 触摸：单击反馈、双击复位、三连击触发 Angry、长按复位、上滑撒娇、下滑睡觉
- 芯片温度保护：≥70°C 强制降入 IDLE，≥80°C 强制进入 SLEEP
- 事件驱动渲染：画面静止时停止 SPI 传输，降低待机功耗

## 快速开始

需要先安装 [PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI）。

```bash
pio run
pio run --target upload
pio device monitor -b 115200
pio test -e native
```

`TFT_eSPI` 已通过 `lib_deps` 自动下载，屏幕引脚和构建开关都在 `platformio.ini` 中，无需修改 `User_Setup.h`。

## 硬件接线

| 组件 | 型号 |
|------|------|
| MCU | ESP32（ESP32-WROOM-32 系列） |
| 屏幕 | ST7796 SPI TFT，320×240 |
| 触摸 | XPT2046，与 TFT 共用 SPI 总线 |
| 背光 | GPIO27（低电平关，高电平开） |

| 信号 | GPIO |
|------|------|
| TFT_SCLK / TOUCH_CLK | GPIO14 |
| TFT_MOSI / TOUCH_MOSI | GPIO13 |
| TFT_MISO / TOUCH_MISO | GPIO12 |
| TFT_CS | GPIO15 |
| TFT_DC | GPIO2 |
| TFT_RST | GPIO4 |
| TFT_BL | GPIO27 |
| TOUCH_CS | GPIO33 |

SPI 频率为 60MHz，RGB 顺序为 `TFT_BGR`。

## 渲染管线

每一帧按顺序经过 5 级变换，输出链式传递：

```text
Transition → Transformation → Variation1 → Variation2 → Blink → FinalConfig → EyeDrawer::Draw()
```

- `Transition`：表情切换，500ms 从 Snapshot 插值到目标配置
- `Transformation`：视线方向与透视缩放，200ms 平滑过渡
- `Variation`：呼吸、脉冲等微表情
- `Blink`：眨眼，100ms → 50ms → 100ms 的梯形波形
- `EyeDrawer`：浮点贝塞尔边界 + 扫描线填充，写入 320×120 的 TFT_eSprite 双缓冲后一次性推送到屏幕

## 电源管理

| 状态 | 进入条件（距上次交互） | 背光 | 刷新率 | 表情行为 |
|------|------------------------|------|--------|----------|
| ACTIVE | 0–60 秒 | 100% | 25ms（40fps） | 完整动画 + 随机表情 |
| RELAX | 60 秒 | 50% | 33ms（30fps） | 固定表情、慢速视线、偶发哈欠 |
| IDLE | 3 分钟 | 25% | 41ms（约 24fps） | 固定 Normal、视线回中、偶发眨眼 |
| SLEEP | 5 分钟 | 0% | 不渲染，200ms 触摸轮询 | Sleepy 闭眼静态画面 |

任意触摸都会回到 ACTIVE 并重置空闲计时。背光使用 Gamma 2.2 感知曲线，状态切换时以缓动曲线平滑渐变（初始快、接近目标慢），避免亮度跳变。
CPU 频率随状态调整：ACTIVE 240MHz、RELAX 160MHz、IDLE 80MHz、SLEEP 80MHz；进入 SLEEP 时发送 `DISPOFF (0x28)`，唤醒时发送 `DISPON (0x29)` 降低面板功耗。

## 触摸手势

| 手势 | 动作 |
|------|------|
| 单击 | 快速眨眼作为反馈，重置空闲计时 |
| 双击（300ms 内） | 回到 Normal 表情 |
| 三连击（1.5s 内） | 触发 Angry 表情 |
| 长按（2s） | 回到 Normal 表情 |
| 上滑 | 撒娇（Coquette） |
| 下滑 | 进入睡眠模式（Sleepy + Zzz），约 2 秒后切到低功耗 IDLE，再次触摸唤醒 |

按压力度阈值默认 `TOUCH_PRESSURE_THRESHOLD = 350`，不同面板可能需要微调。

## 构建开关

以下开关都定义在 `platformio.ini`：

| 开关 | 默认 | 说明 |
|------|------|------|
| `POWER_BATTERY_SENSE_ENABLED` | 0 | GPIO34 接好 2:1 分压后改为 1，启用电池电压采样 |
| `POWER_LIGHT_SLEEP_ENABLED` | 0 | GPIO36 补 10kΩ 上拉后改为 1，启用 LightSleep |
| `POWER_LIGHT_SENSOR_ENABLED` | 0 | GPIO35 光敏自动调光（实测动态范围偏窄，默认停用） |
| `POWER_TEMP_PROTECT_ENABLED` | 1 | 芯片温度保护 |
| `SERIAL_DEBUG_ENABLED` | 1 | 串口调试日志；发布固件可改为 0 关闭 |

## 项目结构

```text
ESP32-demo/
├── .github/workflows/
│   └── ci.yml
├── platformio.ini
├── LICENSE
├── README.md
├── assets/readme/
│   └── hero.svg
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

- LightSleep 唤醒依赖 GPIO36 触摸 IRQ，目前需补上拉电阻并显式开启构建开关
- 电池电压采样需要 GPIO34 接两个 100kΩ 分压电阻
- GPIO35 光敏电阻动态范围偏窄，自动调光默认关闭
- 触摸阈值和 SPI 频率可能因具体面板需要调整

## 许可

本项目基于 playfultechnology 的 ESP32 OLED Emotive Eyes 移植改造，原始项目由
Alastair Aitchison 发布，遵循 MIT 许可。本仓库以 GPL-3.0 发布，完整条款见
`LICENSE`。

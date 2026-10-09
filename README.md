# ESP32 Emotive Eyes

> **固件版本 2.0.0** · 当前验证板型：HW-950 / ESP32-2432S024R

[![PlatformIO CI](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml/badge.svg)](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml)

<p align="center">
  <img src="./assets/readme/hero.svg" width="100%" alt="Emotive Eyes 2.0.0：ESP32 触摸屏上的过程式眼睛表情，包含三种固件配置和八点边缘覆盖渲染">
</p>

Emotive Eyes 为 ESP32-2432S024R 的 ST7796 触摸屏实时绘制表情眼睛。眼睛轮廓由程序计算，不依赖预渲染图片；项目提供常亮、电池省电和循环表情测试三种固件。

## 2.0.0 固件配置

三个环境共用当前表情几何和八点边缘覆盖渲染。`pio run` 默认选择常亮固件；循环测试固件使用固定表情序列。

| PlatformIO 环境 | 用途 | 帧周期 / 目标节奏 | CPU | 空闲策略 |
|---|---|---:|---:|---|
| `esp32dev_always_on` | 默认常亮固件 | 16ms / 约 60fps 目标 | 80MHz | 不因无触摸自动变暗或熄屏 |
| `esp32dev_battery` | 电池省电固件 | ACTIVE 16ms、RELAX 22ms、IDLE 33ms | 80 / 80 / 40MHz | 60秒进入 RELAX，3分钟进入 IDLE，5分钟进入 SLEEP |
| `esp32dev_expression_test` | 循环表情测试 | 16ms / 约 60fps 目标 | 240MHz | 每 450ms 切换表情并输出性能日志 |

帧周期是程序调度目标，不是屏幕面板的实测刷新率。16ms 对应每秒最多约 62.5 次循环；持续动画的历史短测数据见[验证记录](docs/verification/README.md)。

## 快速构建与烧录

先安装 [PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI），连接 HW-950，再运行：

```bash
pio run
pio device list
pio run -t upload --upload-port YOUR_PORT
```

`YOUR_PORT` 替换为设备的串口，例如 Windows 上的 `COM7`。默认命令构建并烧录 `esp32dev_always_on`。选择另外两种固件时，明确指定环境：

```bash
pio run -e esp32dev_battery -t upload --upload-port YOUR_PORT
pio run -e esp32dev_expression_test -t upload --upload-port YOUR_PORT
```

只有表情测试固件默认启用串口调试。监视串口可运行 `pio device monitor --port YOUR_PORT --baud 115200`。

## 表情与屏幕渲染

- 19 种过程式表情；表情切换采用 500ms Smoothstep 插值。
- 默认 Normal 眼型为 60×60 像素，双眼间距参数为 15 像素。
- 眼睛轮廓由圆角多边形和扫描线生成。凸多边形走双边链扫描路径，边缘使用 8 个垂直采样点估算覆盖率，以减轻曲线锯齿。
- 只在画面变化时重绘相关区域，静止时跳过重复屏幕推送。
- 常亮与电池固件的活动动画中，微小注视漂移与较快扫视使用不同速度；眨眼或表情过渡期间会推迟眼神移动，活跃度随表情变化。
- 当前显示为偏暖的浅色背景。渲染优先使用整屏 Sprite；连续内存不足时自动改用两个半屏缓冲。

完整实现说明见[技术文档](docs/技术文档.md)。

## 触摸操作

| 操作 | 结果 |
|---|---|
| 单击 | 快速眨眼反馈 |
| 双击 | 回到 Normal |
| 三连击 | 触发 Angry |
| 长按 2 秒 | 回到 Normal |
| 上滑 | Coquette；700ms 内再次上滑触发 Glee → Awe → Happy 庆祝表情 |
| 左滑 / 右滑 | 亮度每次降低 / 提高 10%，范围 20–100%，设置保存在 NVS |
| 下滑 | 眼睛进入 Sleepy 并显示月牙星光；2 秒内亮度平滑降到 IDLE 档，再次触摸唤醒 |
| BOOT 长按约 1 秒后松开 | 进入软件 `SOFT_OFF` 深睡；按 RST/EN 重启 |

触摸压力阈值默认 600，释放迟滞阈值 400；按下和松开都需要连续两次采样确认。不同面板可能需要校准阈值。

## 电源行为

省电状态机仅由 `esp32dev_battery` 自动按空闲时间运行。RELAX 和 IDLE 的背光目标分别是用户亮度的 50% 和 25%（最低 20%）；SLEEP 关闭背光并停止绘制，以 50ms 周期检查触摸。芯片温度达到 70°C 时，ACTIVE/RELAX 会降至 IDLE；达到 80°C 时进入 SLEEP；降至 65°C 后解除温度保护。

`SOFT_OFF` 会关闭屏幕和背光并让 ESP32 进入 Deep Sleep，不等于物理切断电池。Deep Sleep 未配置按键唤醒，使用 RST/EN 重启；需要长时间断电时使用板载电池开关 SW1。电池电压采样、整机电流和续航目前没有实测，细节见[电池供电诊断](docs/电池供电诊断.md)。

## 硬件与外壳

| 项目 | 当前配置 |
|---|---|
| 主控 | HW-950 / ESP32-2432S024R，ESP32-WROOM-32 |
| 屏幕 | ST7796 SPI TFT，逻辑分辨率 320×240 |
| 触摸 | XPT2046，与 TFT 共用 SPI 总线 |
| SPI | TFT 80MHz，触摸 2.5MHz |
| 背光 | GPIO27 |

板级引脚、接线和限制见[引脚参考手册](docs/ESP32-WROOM-32_引脚参考手册.md)与[引脚功能图解](docs/ESP32-WROOM-32_引脚功能图解.md)。STEP 和 STL 外壳文件在 [`case/`](case/)。

## 文档

- [文档导航与版本说明](docs/README.md)
- [技术文档](docs/技术文档.md)
- [电池供电诊断](docs/电池供电诊断.md)
- [短时帧率与状态验证记录](docs/verification/README.md)
- [版本变更记录](CHANGELOG.md)

## 已知限制

- 设备没有已验证的电池电压采样通路；固件不报告电量或充电状态。
- 当前帧率证据来自固件计时日志，不代表面板物理刷新率，也不能代替长时间稳定性或电流测试。
- 光敏输入 GPIO34 未用于自动亮度调节；Light Sleep 默认关闭。

## 许可与来源

本项目基于 playfultechnology 的 ESP32 OLED Emotive Eyes 移植改造。上游由 Alastair Aitchison 发布并遵循 MIT 许可；本仓库以 GPL-3.0 发布，完整条款见 [`LICENSE`](LICENSE)。

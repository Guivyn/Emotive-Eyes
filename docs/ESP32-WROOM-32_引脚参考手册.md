# HW-950 / ESP32-2432S024R 引脚与功能参考

> 本文档面向仓库当前使用的 HW-950 板型。CYD 外观相近的开发板可能有不同的屏幕、引脚和供电电路；接线前请核对板卡型号与 PCB 丝印。项目当前引脚配置见 `src/board_config.h`。

## 一、板载资源

| 项目 | 说明 |
|------|------|
| 板型 | HW-950 / ESP32-2432S024R |
| 主控模块 | ESP-WROOM-32 |
| 板载屏幕 | 2.4" 电阻触摸屏，分辨率 320×240 (ST7796 驱动) |
| 触摸控制器 | XPT2046 |
| USB 接口 | Type-C，用于供电和串口连接 |
| 电池接口 | 独立 BAT+/BAT− 端子；SW1 控制电池供电 |
| 拓展接口 | 1.25mm 间距排针（I2C、串口等） |
| 存储扩展 | TF 卡槽 (SPI) |
| 音频 | 板载功放与喇叭接口 |

电池充电芯片型号、充电电流和整机电池供电路径尚未通过原理图或实测确认，相关限制见[电池供电诊断](电池供电诊断.md)。

## 二、本项目的运行配置

| 参数 | 规格 |
|------|------|
| 默认 CPU | 常亮与电池固件 ACTIVE 档 80MHz |
| 表情测试 CPU | `esp32dev_expression_test` 使用 240MHz |
| 屏幕逻辑分辨率 | 320×240，横屏 |
| TFT SPI 时钟 | 80MHz |
| 触摸 SPI 时钟 | 2.5MHz |
| 构建方式 | PlatformIO；环境和烧录命令见根目录 README |

---

## 三、板载按键

| 按键 | 功能 |
|------|------|
| RST | 复位 |
| BOOT | 刷机/引导模式 |

---

## 四、外设引脚映射

### 4.1 显示屏 — ST7796（屏幕排线）

| 信号 | GPIO |
|------|------|
| TFT_MISO | GPIO12 |
| TFT_MOSI | GPIO13 |
| TFT_SCLK | GPIO14 |
| TFT_CS   | GPIO15 |
| TFT_DC   | GPIO2  |
| TFT_BL（背光） | GPIO27 |

> 本项目将 `TFT_RST` 设为 `-1`，不占用 GPIO。HW-950 的 GPIO4 接板载 RGB LED；不要将 `TFT_RST` 改配为 GPIO4。

### 4.1.1 触摸控制器 — XPT2046（与 TFT 共用 SPI）

| 信号 | GPIO | 备注 |
|------|------|------|
| TOUCH_CS   | GPIO33 | 片选 |
| TOUCH_CLK  | GPIO14 | 与 TFT_SCLK 共用 |
| TOUCH_MOSI | GPIO13 | 与 TFT_MOSI 共用 |
| TOUCH_MISO | GPIO12 | 与 TFT_MISO 共用 |
| TOUCH_IRQ  | 未接入 | 常规模式通过 SPI Z 轴压力判断按下 |

> 当前固件只有在启用 `POWER_LIGHT_SLEEP_ENABLED=1` 时才使用 GPIO36 作为可选的触摸唤醒信号；GPIO36 没有内部上拉，需要外接 10kΩ 上拉到 3.3V。

### 4.2 TF 卡模块 — SPI

| 信号 | GPIO |
|------|------|
| CS    | GPIO5  |
| MOSI  | GPIO23 |
| CLK   | GPIO18 |
| MISO  | GPIO19 |

### 4.3 音频与指示灯

| 功能 | GPIO |
|------|------|
| 音频输出 (AUDIO) | GPIO26 |
| 喇叭接口 | 2p 1.25mm，≤ 2.5W / 4Ω |
| RGB LED | GPIO17 / GPIO4 / GPIO16 |
| 光敏电阻 | GPIO34 (ADC1_CH6, Input Only) |

> **GPIO4 使用说明**：HW-950 将 GPIO4 接到 RGB LED 的绿色通道。本项目的 `TFT_RST=-1`，所以当前 TFT 配置不会使用 GPIO4；若迁移到其他板型，应先核对屏幕复位线和 LED 接线。

---

## 五、扩展接口（1.25mm 间距排针）

### 5.1 拓展 IO — 接口 1（4p）

| 引脚 | 信号 |
|------|------|
| 1 | GND |
| 2 | IO35 — Input Only / ADC1_CH7 |
| 3 | IO22 — I2C SCL |
| 4 | IO21 — I2C SDA |

### 5.2 拓展 IO — 接口 2（4p）

| 引脚 | 信号 |
|------|------|
| 1 | GND |
| 2 | IO22 — I2C SCL |
| 3 | IO21 — I2C SDA |
| 4 | 3.3V |

> 接口 1 与接口 2 的 I2C 引脚相同（IO22 = SCL，IO21 = SDA），可灵活使用。

### 5.3 串口连接（4p）

| 引脚 | 信号 |
|------|------|
| 1 | 5V |
| 2 | TX — GPIO1 |
| 3 | RX — GPIO3 |
| 4 | GND |

---

## 六、供电

| 方式 | 说明 |
|------|------|
| Type-C | USB 数据/串口连接与供电，可用于烧录程序 |
| 电池端子 | 外接电池接口；充电电路型号与参数尚未实测确认 |
| SW1 | 板级电源开关；随板说明书标注单击开机、双击关机 |

---

## 七、已占用 GPIO 汇总

| GPIO | 功能 | 备注 |
|------|------|------|
| GPIO1 | UART TX | 串口输出 |
| GPIO3 | UART RX | 串口输入 |
| GPIO2 | TFT_DC | 显示 |
| GPIO4 | RGB LED 绿色通道 | 本项目 `TFT_RST=-1`，未与屏幕复位脚复用 |
| GPIO5 | TF_CS | TF 卡 |
| GPIO12 | TFT_MISO / TOUCH_MISO | 共用 SPI |
| GPIO13 | TFT_MOSI / TOUCH_MOSI | 共用 SPI |
| GPIO14 | TFT_SCLK / TOUCH_CLK | 共用 SPI |
| GPIO15 | TFT_CS | 显示 |
| GPIO16 | RGB LED | 指示灯 |
| GPIO17 | RGB LED | 指示灯 |
| GPIO18 | TF_CLK | TF 卡 |
| GPIO19 | TF_MISO | TF 卡 |
| GPIO21 | I2C SDA | 拓展接口 |
| GPIO22 | I2C SCL | 拓展接口 |
| GPIO23 | TF_MOSI | TF 卡 |
| GPIO26 | AUDIO | 音频功放输出 |
| GPIO27 | TFT_BL | 显示背光 |
| GPIO33 | TOUCH_CS | 触控片选 |
| GPIO36 | TOUCH_IRQ（可选） | 仅 LightSleep 唤醒使用，需外接 10kΩ 上拉 |
| GPIO39 | 当前固件未使用 | Input Only |
| GPIO34 | 光敏电阻 | Input Only / ADC1_CH6 |
| GPIO35 | 拓展 IO | Input Only / ADC1_CH7 |

---

## 八、硬件注意事项 ⚠️

### 8.1 Input-Only 引脚限制

ESP32 的 GPIO34、GPIO35、GPIO36（SENSOR_VP）、GPIO39（SENSOR_VN）是**仅输入引脚**：

- 不能配置为 OUTPUT
- 没有内部上拉/下拉电阻
- 代码中只能使用 `pinMode(x, INPUT)` + `digitalRead(x)`
- 绝对不能执行 `digitalWrite` 或 `pinMode(x, OUTPUT)`

### 8.2 GPIO36 LightSleep 唤醒信号（可选）

常规运行模式不依赖 XPT2046 的 IRQ，而是通过 SPI 读取 Z 轴压力值判断触摸。启用 `POWER_LIGHT_SLEEP_ENABLED=1` 后，GPIO36 作为 RTC 唤醒输入；由于它没有内部上拉，必须外接 10kΩ 上拉到 3.3V：

- 无外部上拉时可能随机跳变，导致 LightSleep 唤醒行为不可靠
- 未启用 LightSleep 时无需连接该信号，触摸仍通过共享 SPI 工作

### 8.3 GPIO4 与 TFT 复位配置

HW-950 的 GPIO4 接到 RGB LED 绿色通道。本项目将 `TFT_RST` 配置为 `-1`，因此不会驱动 GPIO4 作为屏幕复位脚。若改用其他板型或自行修改 TFT 配置，应先核对 PCB 接线，避免把 TFT 复位信号接到被 LED 使用的 GPIO。

### 8.4 触摸 SPI 引脚

触摸控制器 XPT2046 与 TFT 共用 SPI 信号线，仅片选独立：

- TOUCH_CS (GPIO33)、TOUCH_CLK (GPIO14)、TOUCH_MOSI (GPIO13)、TOUCH_MISO (GPIO12)
- 常规模式不接入 IRQ；启用 LightSleep 时才需要 GPIO36 + 外部上拉

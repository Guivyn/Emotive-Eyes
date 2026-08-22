# ESP32-WROOM-32 多功能开发板 — 引脚与功能参考手册

---

## 一、产品概览

| 项目 | 说明 |
|------|------|
| 产品名称 | ESP32 WiFi+BT 双模开发板 |
| 主控模块 | ESP-WROOM-32 |
| 尺寸 | 28.5 mm × 51.8 mm |
| 板载屏幕 | 2.4" 电阻触摸屏，分辨率 320×240 (ST7796 驱动) |
| 烧录接口 | Type-C |
| 供电方式 | Type-C 供电 / 外接电池 |
| 电池开关 | 点按一次开机，点按两次关机 |
| 拓展接口 | 1.25mm 间距排针（I2C、串口等） |
| 存储扩展 | TF 卡槽 (SPI) |
| 音频 | 功放芯片 + 喇叭接口 (2p 1.25mm，支持 ≤ 2.5W / 4Ω) |

---

## 二、ESP-WROOM-32 模块参数特性

| 参数 | 规格 |
|------|------|
| Wi-Fi | 802.11 b/g/n，支持 STA / AP / STA+AP 模式 |
| 蓝牙 | 蓝牙 4.2 BR/EDR + BLE |
| CPU | 双核 32 位 Xtensa LX6，主频最高 240 MHz |
| 算力 | 600 DMIPS |
| SRAM | 520 KB |
| SPI Flash | 默认 32 Mbit |
| 支持接口 | UART、SPI、SDIO、I2C、I2S、PWM、IR、ADC、DAC |
| 片上传感器 | 霍尔传感器、温度传感器、电容触摸传感器 |
| IO 口数量 | 22 个 |
| 串口速率 | 默认 115200 bps |
| 频谱范围 | 2412 ~ 2484 MHz |
| 天线 | 板载 PCB 天线，增益 2 dBi |
| 发射功率 | 802.11b: 17±2 dBm (@11 Mbps)；802.11g: 14±2 dBm (@54 Mbps)；802.11n: 13±2 dBm (@MCS7) |
| 接收灵敏度 | CCK 1 Mbps: -90 dBm；CCK 11 Mbps: -85 dBm；6 Mbps: -88 dBm；54 Mbps: -70 dBm；MCS7: -67 dBm |
| 安全性 | WPA / WPA2 / WPA2-Enterprise / WPS |
| 休眠 | 深度休眠电流低至 6.5 μA |
| 固件升级 | 本地串口升级 / 远程 FOTA |
| 配网方式 | Smart Config、AirKiss 一键配网 |
| 软件生态 | 内嵌 LwIP、FreeRTOS，支持通用 AT 指令 |
| 调试接口 | OpenOCD |

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

> **注意**：GPIO4 在此开发板上同时被 RGB LED 占用。如果 TFT_eSPI 驱动将 TFT_RST 配置为 GPIO4，会导致电平冲突。建议将 TFT_RST 设为 -1（不使用软复位），或换用空闲 GPIO。

### 4.1.1 触摸控制器 — XPT2046（独立软 SPI）

| 信号 | GPIO | 备注 |
|------|------|------|
| TOUCH_CS   | GPIO33 | 片选 |
| TOUCH_CLK  | GPIO25 | 时钟 |
| TOUCH_MOSI | GPIO32 | 数据输出（MCU→XPT2046） |
| TOUCH_MISO | GPIO39 | 数据输入（XPT2046→MCU）**⚠️ Input Only，严禁写操作** |
| TOUCH_IRQ  | GPIO36 | 触摸中断（按下时拉低）**⚠️ Input Only，无内部上拉** |

> **关键硬件警告**：GPIO36（SENSOR_VP）和 GPIO39（SENSOR_VN）是 ESP32 的 Input-Only 引脚——
> 内部没有上拉/下拉电阻，也不能配置为 OUTPUT。代码中只能使用 `digitalRead`，绝对不能对这两个引脚执行 `digitalWrite` 或 `pinMode(x, OUTPUT)`。

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

> **GPIO4 复用冲突**：GPIO4 同时作为 RGB LED 通道和 TFT_RST（显示屏复位）。在 TFT_eSPI 的 `User_Setup.h` 中如定义 `TFT_RST 4`，初始化时拉低/拉高操作会导致 LED 闪烁。优先方案：将 TFT_RST 设为 -1 禁用软复位，利用屏幕模组的硬件上电复位。

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
| Type-C | 数据传输 + 供电，支持烧录程序 |
| 电池端子 | 连接外接电池，支持充电 |
| 电池开关 | 点按一次 → 开机；点按两次 → 关机 |

---

## 七、已占用 GPIO 汇总

| GPIO | 功能 | 备注 |
|------|------|------|
| GPIO1 | UART TX | 串口输出 |
| GPIO3 | UART RX | 串口输入 |
| GPIO2 | TFT_DC | 显示 |
| GPIO4 | RGB LED / TFT_RST | ⚠️ 与 TFT_RST 共用，见 4.3 节 |
| GPIO25 | TOUCH_CLK | 触摸 SPI 时钟 |
| GPIO32 | TOUCH_MOSI | 触摸 SPI 数据输出 |
| GPIO5 | TF_CS | TF 卡 |
| GPIO12 | TFT_MISO | 显示 |
| GPIO13 | TFT_MOSI | 显示 |
| GPIO14 | TFT_SCLK | 显示 |
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
| GPIO36 | TOUCH_IRQ | 触摸中断 ⚠️ Input Only，无内部上拉 |
| GPIO39 | TOUCH_MISO | 触摸 SPI 数据输入 ⚠️ Input Only |
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

### 8.2 GPIO36 触摸中断悬空风险

XPT2046 的 IRQ 引脚（GPIO36）是开漏输出，按下时拉低，释放时高阻态。由于 GPIO36 没有内部上拉，**在无外部上拉电阻时电平不可靠**：

- 可能一直读到 LOW，导致触摸状态机认为屏幕始终被按下
- 可能随机跳变，导致误触发
- **解决方式**：在 IRQ 引脚焊接 10kΩ 上拉电阻至 3.3V；或软件层面绕过 IRQ，改用 SPI 读取 Z 轴压力值判断是否按下

### 8.3 GPIO4 复用冲突

GPIO4 同时连接 RGB LED 和 TFT 复位引脚（TFT_RST）。如果 `User_Setup.h` 中定义 `TFT_RST 4`：

- TFT_eSPI 初始化时拉低 RST 会导致 LED 异常闪烁
- LED 驱动的电平变化可能干扰屏幕复位时序
- **建议**：将 `TFT_RST` 设为 `-1`（禁用软复位），依赖屏幕模组的硬件上电复位

### 8.4 触摸 SPI 引脚

触摸控制器 XPT2046 使用独立的软 SPI 总线，不与 TFT 的硬件 SPI 共享：

- TOUCH_CS (GPIO33)、TOUCH_CLK (GPIO25)、TOUCH_MOSI (GPIO32)、TOUCH_MISO (GPIO39)
- MISO 和 IRQ 使用了 Input-Only 引脚，驱动代码必须确保只读不写

# HW-950 / ESP32-2432S024R 开发板 — 引脚功能图解

> 下方 SVG 示意图标注了主要引脚和板载外设的位置关系（非实际尺寸比例）。

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 900 440" font-family="Consolas, monospace" font-size="11">
  <defs>
    <filter id="shadow" x="-2%" y="-2%" width="104%" height="104%">
      <feDropShadow dx="1" dy="1" stdDeviation="1.5" flood-opacity="0.15"/>
    </filter>
  </defs>

  <!-- 开发板轮廓 -->
  <rect x="50" y="40" width="800" height="340" rx="8" fill="#1a2a4a" stroke="#2a4a7a" stroke-width="2" filter="url(#shadow)"/>

  <!-- 标题 -->
  <text x="450" y="65" text-anchor="middle" fill="#8ab4f8" font-size="15" font-weight="bold">HW-950 / ESP32-2432S024R — 引脚参考图</text>

  <!-- ESP32-WROOM-32 模块（居中凸出） -->
  <rect x="280" y="88" width="340" height="68" rx="6" fill="#2d5a1e" stroke="#4a9a30" stroke-width="1.5"/>
  <text x="450" y="112" text-anchor="middle" fill="#b8e6a0" font-size="13" font-weight="bold">ESP32-WROOM-32</text>
  <text x="450" y="130" text-anchor="middle" fill="#8ac070" font-size="10">双核 240MHz · 520KB SRAM · WiFi + BLE 4.2</text>
  <text x="450" y="146" text-anchor="middle" fill="#8ac070" font-size="10">SPI Flash 32Mbit · PCB 天线 2dBi</text>

  <!-- ===== 左侧引脚列 ===== -->
  <!-- GND / 电源 -->
  <rect x="30" y="100" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="22" y="112" text-anchor="end" fill="#ccc" font-size="9">GND</text>

  <rect x="30" y="130" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="22" y="142" text-anchor="end" fill="#ccc" font-size="9">3V3</text>

  <rect x="30" y="160" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="22" y="172" text-anchor="end" fill="#ccc" font-size="9">5V</text>

  <!-- 左侧底部 → RST / BOOT 按键 -->
  <rect x="60" y="330" width="100" height="28" rx="5" fill="#3a3a5a" stroke="#6a6a8a" stroke-width="1.5"/>
  <text x="110" y="348" text-anchor="middle" fill="#c0c0e0" font-size="10" font-weight="bold">RST</text>

  <rect x="180" y="330" width="100" height="28" rx="5" fill="#3a3a5a" stroke="#6a6a8a" stroke-width="1.5"/>
  <text x="230" y="348" text-anchor="middle" fill="#c0c0e0" font-size="10" font-weight="bold">BOOT</text>

  <!-- ===== 右侧引脚列 ===== -->
  <rect x="850" y="100" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="858" y="112" text-anchor="start" fill="#ccc" font-size="9">EN</text>

  <rect x="850" y="130" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="858" y="142" text-anchor="start" fill="#ccc" font-size="9">IO23</text>

  <rect x="850" y="160" width="20" height="18" rx="2" fill="#555" stroke="#888"/>
  <text x="858" y="172" text-anchor="start" fill="#ccc" font-size="9">IO22</text>

  <!-- ===== 底部：端子、Type-C、电池接口 ===== -->
  <!-- Type-C -->
  <rect x="340" y="355" width="120" height="20" rx="3" fill="#444" stroke="#888" stroke-width="1"/>
  <text x="400" y="368" text-anchor="middle" fill="#ddd" font-size="9">Type-C (USB/UART)</text>

  <!-- 电池端子 -->
  <rect x="520" y="355" width="120" height="20" rx="3" fill="#4a3a2a" stroke="#8a6a4a" stroke-width="1"/>
  <text x="580" y="368" text-anchor="middle" fill="#dda" font-size="9">电池端子</text>

  <!-- ===== 板载外设标注 ===== -->

  <!-- 屏幕 -->
  <rect x="200" y="180" width="500" height="100" rx="4" fill="#1a3a3a" stroke="#3a7a7a" stroke-width="1.5" stroke-dasharray="5,3"/>
  <text x="450" y="220" text-anchor="middle" fill="#5ababa" font-size="12" font-weight="bold">2.4" ST7796 触摸屏 (320×240)</text>
  <text x="450" y="240" text-anchor="middle" fill="#4a9a9a" font-size="10">TFT: IO12(MISO) · IO13(MOSI) · IO14(SCLK) · IO15(CS) · IO2(DC) · IO27(BL)</text>
  <text x="450" y="253" text-anchor="middle" fill="#d4a040" font-size="9">TOUCH (XPT2046, shared SPI): IO33(CS) · IO14(CLK) · IO13(MOSI) · IO12(MISO) · IRQ optional IO36</text>

  <!-- TF 卡槽 -->
  <rect x="670" y="88" width="100" height="50" rx="3" fill="#2a3a2a" stroke="#5a8a5a" stroke-width="1"/>
  <text x="720" y="108" text-anchor="middle" fill="#8aba8a" font-size="10">TF 卡槽 (SPI)</text>
  <text x="720" y="124" text-anchor="middle" fill="#6a9a6a" font-size="9">IO5/23/18/19</text>

  <!-- RGB LED -->
  <circle cx="100" cy="220" r="12" fill="#333" stroke="#666" stroke-width="1.5"/>
  <text x="100" y="224" text-anchor="middle" fill="#aaa" font-size="8">RGB</text>
  <text x="100" y="240" text-anchor="middle" fill="#888" font-size="8">IO17/4/16</text>

  <!-- 光敏电阻 -->
  <circle cx="100" cy="280" r="10" fill="#4a3a2a" stroke="#8a7a4a" stroke-width="1"/>
  <text x="100" y="284" text-anchor="middle" fill="#cca" font-size="7">LDR</text>
  <text x="100" y="298" text-anchor="middle" fill="#888" font-size="8">IO34</text>

  <!-- 喇叭接口 -->
  <rect x="670" y="265" width="100" height="30" rx="3" fill="#3a2a4a" stroke="#7a4a8a" stroke-width="1"/>
  <text x="720" y="280" text-anchor="middle" fill="#ba8aba" font-size="9">喇叭接口 (2P)</text>
  <text x="720" y="292" text-anchor="middle" fill="#9a6a9a" font-size="8">2.5W/4Ω · IO26</text>

  <!-- ===== 底部扩展接口 ===== -->

  <!-- 拓展接口 1 -->
  <rect x="60" y="120" width="80" height="18" rx="2" fill="#3a3a5a" stroke="#6a6a9a" stroke-width="1"/>
  <text x="100" y="132" text-anchor="middle" fill="#babaee" font-size="9">I2C (IO21/22)</text>

  <!-- 拓展接口 2 -->
  <rect x="760" y="120" width="80" height="18" rx="2" fill="#3a3a5a" stroke="#6a6a9a" stroke-width="1"/>
  <text x="800" y="132" text-anchor="middle" fill="#babaee" font-size="9">UART (IO1/3)</text>

  <!-- 音频功放 -->
  <rect x="670" y="300" width="100" height="20" rx="3" fill="#3a2a3a" stroke="#6a4a6a" stroke-width="1"/>
  <text x="720" y="314" text-anchor="middle" fill="#ba8a9a" font-size="9">功放芯片</text>

  <!-- 图例 -->
  <rect x="750" y="332" width="12" height="12" rx="1" fill="#2d5a1e" stroke="#4a9a30" stroke-width="1"/>
  <text x="766" y="342" fill="#aaa" font-size="9">= 主控模块</text>
  <rect x="750" y="348" width="12" height="12" rx="1" fill="#1a3a3a" stroke="#3a7a7a" stroke-width="1"/>
  <text x="766" y="358" fill="#aaa" font-size="9">= 外设</text>
  <rect x="750" y="364" width="12" height="12" rx="1" fill="#3a3a5a" stroke="#6a6a9a" stroke-width="1"/>
  <text x="766" y="374" fill="#aaa" font-size="9">= 接口/按键</text>
</svg>
```

> 注意：上图仅为引脚功能示意布局，不反映实际 PCB 物理排列。实际焊接与接线请以 PCB 丝印为准。
>
> **⚡ 触摸控制器 (XPT2046)**：当前固件使用与 TFT 共用的 IO12/13/14，并通过 SPI Z 轴压力判断按下，不依赖 IRQ。只有启用 `POWER_LIGHT_SLEEP_ENABLED=1` 时，才需要将 IRQ 接到 IO36，并补 10kΩ 上拉至 3.3V。
>
> **GPIO4 使用说明**：HW-950 的 GPIO4 接 RGB LED 绿色通道；本项目将 `TFT_RST` 配置为 `-1`，不会用 GPIO4 复位屏幕。迁移到其他板型时请重新核对接线。

---

## 八、项目配置与引脚复用

本项目使用 PlatformIO 构建，构建环境和烧录命令见根目录 [README](../README.md)。本图是硬件引脚示意，不是通用 Arduino、ESP-IDF 或 MicroPython 配置指南。

板载外设未使用时，以下 GPIO 可考虑复用；操作前请核对启动绑带、电路连接和 ESP32 的输入/输出限制：

- **GPIO0** — BOOT/启动绑带，电平会影响启动模式。
- **GPIO4 / GPIO16 / GPIO17** — 板载 RGB LED；HW-950 上 GPIO4 为绿色通道。
- **GPIO21 / GPIO22** — I2C 扩展接口。
- **GPIO34 / GPIO35** — 仅输入，可作为 ADC 输入。
- **GPIO26** — 音频输出，未使用板载功放时才考虑复用。

### ADC 通道参考

| ADC | 通道 | GPIO |
|-----|------|------|
| ADC1_CH6 | 6 | GPIO34 |
| ADC1_CH7 | 7 | GPIO35 |
| ADC2 | 若干 | 注意 Wi-Fi 启用时 ADC2 不可用 |

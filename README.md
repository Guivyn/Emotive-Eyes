# ESP32 Emotive Eyes

> 在 HW-950（ESP32-2432S024R）触摸屏开发板上运行的实时表情眼睛固件。眼睛由 ESP32 根据参数即时绘制，不需要预制图片或动画帧。当前固件版本：**2.0.0**。

[![PlatformIO CI](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml/badge.svg)](https://github.com/Guivyn/Emotive-Eyes/actions/workflows/ci.yml)

<p align="center">
  <img src="./assets/readme/hero.svg" width="100%" alt="Emotive Eyes 2.0.0：ESP32 实时绘制表情眼睛、三种固件环境与抗锯齿渲染">
</p>

本项目适合想亲手复现一个小型桌面表情屏、学习 ESP32 屏幕绘图，或研究动画状态机与触摸交互的开发者。按下面的硬件和步骤准备后，可以编译并烧录固件，在屏幕上看到眼睛动画并通过触摸操作。

## 功能一览

- **19 种表情**：Normal、Angry、Glee、Happy、Sad、Worried、Focused、Annoyed、Surprised、Skeptic、Frustrated、Unimpressed、Sleepy、Suspicious、Squint、Furious、Scared、Awe、Coquette。
- **自动眼神与动画**：表情会平滑切换，眼睛会眨动、轻微漂移或快速扫视；动作节奏会随表情变化。
- **触摸交互**：点按、连点、长按和上下左右滑动可以触发表情、调节亮度或改变电源状态。
- **实时抗锯齿绘图**：圆角眼睛轮廓由代码生成，使用覆盖率采样减轻边缘锯齿；静止画面不会重复发送整屏图像。
- **三种固件环境**：常亮、省电和循环表情测试。它们共用同一套表情与绘图代码。

## 硬件

本仓库的引脚配置针对 **HW-950 / ESP32-2432S024R**。这块板集成了 ESP32、ST7796 显示屏和 XPT2046 触摸屏。首次运行只需一块开发板和一根 USB 数据线，不需要外接屏幕、触摸模块或电阻。

| 部件 | 配置 |
|---|---|
| 主控板 | HW-950 / ESP32-2432S024R，ESP32-WROOM-32 系列 |
| 显示屏 | ST7796 SPI TFT，逻辑分辨率 320×240 |
| 触摸屏 | XPT2046，与显示屏共用 SPI 总线 |
| 背光 | GPIO27 |
| TFT SPI | SCLK=GPIO14，MOSI=GPIO13，MISO=GPIO12 |
| TFT 控制 | CS=GPIO15，DC=GPIO2，RST=-1 |
| 触摸片选 | TOUCH_CS=GPIO33 |
| SPI 时钟 | TFT 80MHz，触摸 2.5MHz |
| BOOT 按键 | GPIO0 |

`RST=-1` 表示显示屏复位接在开发板的 RESET 线路上，不需要再分配一个 ESP32 GPIO。TFT_eSPI 的驱动型号、引脚和 SPI 频率已经写入项目配置，不需要修改库目录里的 `User_Setup.h`。

> 使用其他 CYD 或 ESP32 屏幕板时，请先核对屏幕型号、触摸控制器和引脚，再按实际硬件调整 `src/board_config.h` 与 `platformio.ini`。外观相似的板子不一定使用相同接线。

详细引脚资料见[ESP32-WROOM-32 引脚参考手册](docs/ESP32-WROOM-32_引脚参考手册.md)和[引脚功能图解](docs/ESP32-WROOM-32_引脚功能图解.md)。

## 准备开发环境

1. 安装 [Visual Studio Code](https://code.visualstudio.com/)。
2. 在 VS Code 扩展商店安装 **PlatformIO IDE**。
3. 安装 [Git](https://git-scm.com/downloads)，用于下载项目。
4. 准备 HW-950 开发板和可传输数据的 USB 线。

PlatformIO 会根据 `platformio.ini` 自动下载 ESP32 Arduino 构建平台和 TFT_eSPI 库。仓库指定的构建依赖为 Espressif32 **7.0.1**、TFT_eSPI **2.5.43**，框架为 Arduino。

## 从下载到运行

### 1. 下载项目

打开 VS Code 终端或 PlatformIO Core CLI，执行：

~~~bash
git clone https://github.com/Guivyn/Emotive-Eyes.git
cd Emotive-Eyes
~~~

如果已经下载了 ZIP 文件，在 VS Code 中打开包含 `platformio.ini` 的 `Emotive-Eyes` 项目根目录。

### 2. 连接开发板并找到串口

用 USB 数据线连接开发板，然后执行：

~~~bash
pio device list
~~~

记下设备对应的端口。Windows 通常显示为 `COMx`（例如 `COM7`）；macOS 或 Linux 会显示为 `/dev/...` 路径。下面命令中的 `YOUR_PORT` 都要换成实际端口。

### 3. 编译固件

默认环境是常亮固件。首次编译时，PlatformIO 会下载需要的平台和库：

~~~bash
pio run -e esp32dev_always_on
~~~

编译省电版或循环表情测试版时，改用对应环境：

~~~bash
pio run -e esp32dev_battery
pio run -e esp32dev_expression_test
~~~

### 4. 烧录到开发板

以 Windows 的 `COM7` 为例；如果你的端口不同，请替换端口名称：

~~~bash
pio run -e esp32dev_always_on -t upload --upload-port COM7
~~~

烧录其他固件时，把环境名替换为 `esp32dev_battery` 或 `esp32dev_expression_test`。烧录期间请保持开发板连接，不要让其他串口监视器同时占用该端口。

### 5. 查看启动与运行信息

通过串口监视器连接开发板：

~~~bash
pio device monitor --port COM7 --baud 115200
~~~

打开串口监视器后，在屏幕上可以看到闭眼启动画面，约 **500ms** 后睁眼显示 Normal；启动约 **5 秒** 后开始随机视线移动，约 **8 秒** 后开始随机眨眼和表情变化。

常亮和省电固件默认关闭详细调试日志；循环表情测试固件会输出调试与性能信息。交互命令仍可在串口监视器中输入。

## 触摸操作

| 操作 | 结果 |
|---|---|
| 单击 | 眨眼反馈 |
| 双击 | 回到 Normal |
| 三连击 | 触发 Angry；第三次点按需在第二次点按后的 300ms 确认窗口内完成 |
| 长按 2 秒 | 回到 Normal |
| 上滑 | 切换到 Coquette |
| 700ms 内连续上滑两次 | 播放 Glee → Awe → Happy 的庆祝动作，然后回到 Coquette |
| 左滑 / 右滑 | 每次降低 / 提高 10% 背光亮度 |
| 下滑 | 显示 Sleepy 和月牙星光，约 2 秒内渐暗到 IDLE 档；再次触摸唤醒 |

亮度范围为 **20%–100%**，默认 **100%**，触摸滑动或串口调整后的亮度会保存。触摸屏按下压力阈值为 600，松开阈值为 400。触摸长按是 2 秒；板上的 BOOT 按键用于软件待机，操作方式见下方。

## 选择固件环境

在 PlatformIO 中，环境（environment）就是一组构建设置。三个固件共用表情和绘图代码，主要区别是空闲策略、CPU 频率和调试输出。

| 环境 | 用途 | 程序绘制间隔 | CPU 频率 | 行为 |
|---|---|---:|---:|---|
| `esp32dev_always_on` | 日常常亮运行，默认环境 | 16ms（约 60fps 调度目标） | ACTIVE 80MHz | 不按空闲时间自动降低亮度或进入 SLEEP |
| `esp32dev_battery` | 自动省电运行 | ACTIVE 16ms、RELAX 22ms、IDLE 33ms | ACTIVE/RELAX 80MHz、IDLE 40MHz、SLEEP 80MHz | 无交互 60 秒进入 RELAX、3 分钟进入 IDLE、5 分钟进入 SLEEP |
| `esp32dev_expression_test` | 循环观察固定表情序列 | 16ms（约 60fps 调度目标） | 240MHz | 每 450ms 切换一个表情并输出调试信息 |

省电版的 ACTIVE 使用当前亮度，RELAX 将背光目标降至当前亮度的 50%，IDLE 降至 25%（最低 20%），SLEEP 关闭背光并停止绘图、每 50ms 检查一次触摸。触摸会将省电状态唤醒到 ACTIVE。

常亮版和省电版都启用芯片温度保护：达到 **70°C** 时降到 IDLE，达到 **80°C** 时进入 SLEEP，温度降至 **65°C** 后恢复。常亮版仍可通过下滑手动进入 IDLE。

> 16ms、22ms 和 33ms 是固件安排绘图循环的时间间隔，约 60fps、45fps 和 30fps 是相应的程序调度目标；它们不代表显示面板的物理刷新率。

## 电源键与软件待机

运行中按住板上的 **BOOT** 键约 **1 秒**，松开后固件进入 `SOFT_OFF`：关闭显示和背光，并让 ESP32 进入 Deep Sleep。按 **RST/EN** 可以重新启动固件。

`SOFT_OFF` 是软件待机，不会切断电池输出。需要物理关闭开发板电池电源时，使用 HW-950 板上的 **SW1** 电源开关。

## 串口设置

串口速率为 **115200 baud**。在串口监视器中输入命令并发送换行，可以查询或调整设置：

| 命令 | 作用 |
|---|---|
| `help` | 查看可用串口命令 |
| `brightness` | 查询当前亮度 |
| `brightness 80` | 将亮度设为 80%（范围 20–100%） |
| `cal` | 查询触摸校准参数 |
| `cal <offset> <range>` | 设置触摸校准参数并保存 |

亮度保存到 ESP32 的 NVS；触摸校准数据保存在 LittleFS。多数情况下无需改校准参数；如果触摸坐标与画面位置不吻合，可以先用 `cal` 查看当前值，再结合实际触摸结果调整。

## 工作原理

固件每一轮循环大致完成以下工作：

1. `FaceBehavior` 选择表情，`LookAssistant` 决定视线移动目标。
2. `Eye` 将当前表情、视线、微表情和眨眼组合成这一帧的眼睛形状。表情切换使用 **500ms Smoothstep** 平滑过渡；动画以 `millis()` 计时，不依赖固定帧率推进。
3. `EyeDrawer` 根据眼睛参数计算圆角轮廓，并用扫描线填充。边缘使用 **8 点覆盖率采样**减轻锯齿；凸多边形使用双边链扫描线，特殊轮廓使用通用扫描路径。
4. `Face` 将眼睛绘制到显示缓冲区，只发送变化区域。内存允许时使用 320×240 全屏缓冲；否则改用 160×240 分块缓冲；两者都无法申请时直接绘制到 TFT。

这条流程让表情可以通过几何参数改变，而不必为每种表情准备一张图片。完整动画设计与渲染细节见[技术文档](docs/技术文档.md)。

## 项目结构

~~~text
Emotive-Eyes/
├── platformio.ini       # PlatformIO 环境、库依赖和构建参数
├── VERSION              # 固件版本
├── assets/readme/       # README 展示图
├── src/
│   ├── main.cpp         # Arduino setup/loop 与串口命令
│   ├── board_config.h   # HW-950 引脚定义
│   ├── Config.h         # 屏幕、触摸与动画参数
│   ├── Animation.h      # 动画时间曲线
│   ├── Eye.cpp/.h       # 眼睛参数、动画和几何绘制
│   ├── Face.cpp/.h      # 表情、触摸手势和行为
│   ├── PowerButton.h    # BOOT 按键的消抖与长按判断
│   ├── PowerManager.*   # 背光、电源状态与温度保护
│   ├── PowerDiagnostics.* # 串口电源诊断信息
│   ├── CalibrationCodec.h # 触摸校准数据格式
│   ├── Log.h            # 可配置的日志输出
│   ├── Version.h        # 固件版本定义
│   └── FacePower.*      # Face 与电源状态之间的接口
├── test/test_native/    # 可在电脑上运行的 Native 单元测试
├── docs/                # 技术说明和开发板引脚资料
└── case/                # 配套外壳的 STL / STEP 文件
~~~

学习或修改时，常见入口如下：

- 修改屏幕与触摸引脚：`src/board_config.h`
- 修改构建环境、库版本、CPU 与绘图参数：`platformio.ini`
- 修改眼睛轮廓与渲染：`src/Eye.cpp`、`src/Eye.h`
- 修改表情与触摸手势：`src/Face.cpp`、`src/Face.h`
- 修改省电、背光与温度策略：`src/PowerManager.cpp`、`src/PowerManager.h`

## 在电脑上运行逻辑测试

不连接 ESP32，也可以运行 Native 主机测试：

~~~bash
pio test -e native
~~~

固件构建和 Native 测试也由仓库的 PlatformIO CI 工作流执行。

## 许可与来源

本仓库使用 **GPL-3.0**，完整条款见 [LICENSE](LICENSE)。眼睛动画的基础实现移植并改造自 playfultechnology 发布、由 Alastair Aitchison 编写的 ESP32 OLED Emotive Eyes 项目。

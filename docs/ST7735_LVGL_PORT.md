# Cubie A7Z ST7735 与 LVGL 适配

本适配复用 openvela/NuttX 官方 `drivers/lcd/st7735.c`，板级代码只负责
A733 SPI1、D/C 和 RESET 接线。应用通过标准 `/dev/lcd0` 和 LVGL NuttX LCD
后端访问屏幕，没有引入 Linux `spidev` 或 Python 运行时依赖。

## 已验证硬件与接线

| ST7735 | Cubie A7Z | 物理引脚 |
| --- | --- | --- |
| VCC | 3.3 V | 1 或 17 |
| GND | GND | 任一 GND |
| SCK | PD11 / SPI1_CLK | 23 |
| MOSI | PD12 / SPI1_MOSI | 19 |
| CS | PD10 / SPI1_CS0 | 24 |
| DC | PL5 / R_PIO GPIO | 22 |
| RST | PB0 / GPIO | 7 |
| BL | 3.3 V | 1 或 17 |

MISO 不连接。屏幕为 1.8 英寸 128×160 ST7735，SPI Mode 0，RGB565。
当前 A733 polling lower-half 将频率限制为 12 MHz。

## 软件层次

```text
A733 SPI1 lower-half + SPI_CMDDATA(PL5)
  -> 官方 st7735.c
  -> lcd_dev_s 和 /dev/lcd0
  -> 官方 LVGL NuttX LCD backend
  -> 桌宠 UI
```

`a7z_st7735.c` 是板级 glue，通用控制器逻辑仍由官方驱动维护。SPI1
lower-half 同时支持 8-bit 命令和高字节优先的 16-bit RGB565 像素传输。

## 构建

在完整 openvela 工作区根目录对应的比赛仓库中执行：

```bash
bash contest2026_274_Dogking/tools/build-a733.sh
```

构建脚本只在构建期间暂存 overlay，并在退出时恢复官方工作区。

本阶段已完成 WSL 全量构建、最终 ELF 符号检查和镜像内核回读校验。构建结果：

```text
build: quickly-openvela/cmake_out/cubie-a7z_nsh_v120_st7735
nuttx.bin: 2183008 bytes
nuttx.bin SHA-256: f3871b2422ba000e14df016197fe2a09217a58f03f873e62b4e5084c73700eb7
```

可烧录候选镜像（不提交到 Git）：

```text
A733-A7Z-ALL-Files/openvela-a733-cubie-a7z-sd-st7735-lvgl-v120-candidate.img
size: 2147483648 bytes
SHA-256: 6bececd77790eba146de0ef5762a2ec11b296a9665fb08f7f9fe24e67024941c
```

镜像已经通过 ext4、GPT、内嵌内核大小及 SHA-256 回读校验。编译和封装成功不等于
面板实机验收完成；首次烧录后仍需执行下一节命令确认接线、方向和颜色。

## 首次上板检查

上电前再次核对 3.3 V、GND、DC 和 RESET，禁止把屏幕接到 5 V 信号。
启动后检查：

```text
dmesg
ls /dev/lcd0
display status
display test 30
display run
```

预期日志包含：

```text
A733: ST7735 LCD ready at /dev/lcd0 (0)
```

`display test` 使用精简的官方 LVGL 核心绘制 RGB 色条和桌宠脸，不会把完整
LVGL demo 素材集合装入产品固件。`display run` 持续运行，按 `Ctrl+C` 退出。

若画面方向正确但红蓝互换，切换 `CONFIG_LCD_ST7735_BGR` 后重新构建；
若屏幕整体镜像，调整 `CONFIG_LCD_RPORTRAIT`。这两个选项只描述面板差异，
不应在应用层交换颜色或坐标。

## 桌宠接入约束

- 所有 LVGL API 由单一 UI 线程调用。
- Agent、ASR、TTS 和本地 LLM 只向 UI 消息队列发送状态事件。
- 情绪状态应保持到下一次明确更新，避免每句文本完成后闪回 idle。
- 首版使用局部刷新与约 10～15 FPS 动画；DMA/FIFO 优化应保持为后续独立改动。

## 当前验收边界

- 已完成：A733 SPI1、`SPI_CMDDATA`、板级复位、官方 ST7735、`/dev/lcd0`、
  官方 LVGL NuttX LCD backend、显示测试应用、全量编译和镜像封装。
- 待真机：面板点亮、RGB/BGR、旋转方向、连续刷新稳定性和实际帧率。
- 待产品层：将桌宠 Agent 的情绪/动作事件接入单一 LVGL UI 线程。

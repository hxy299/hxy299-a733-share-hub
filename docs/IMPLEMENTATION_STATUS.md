# A733 / Cubie A7Z 适配状态

状态日期：2026-09-10。“已通过”表示已在真实的 4 GiB Cubie A7Z 上观察到，
而不只是完成编译。

## 已验证的系统基线

- Boot0/SCP/BL31/U-Boot 通过 `booti` 将控制权以 EL1 交给 openvela。
- AArch64 运行时、MMU、异常、GICv3 和虚拟定时器正常。
- UART0 提供早期输出，并在 115200 8N1 下提供可交互的 NSH 控制台。
- 4 GiB DRAM 通过分区堆区域暴露，同时保留固件预留区域。
- procfs、`free`、`ps`、`uptime`、`dmesg`、信号和 Ctrl+C 正常。
- `poweroff` 和 `reboot` 使用开发板/PSCI 路径。

## 已验证的存储与外设

- SDMMC0：PIO，400 kHz 初始化，12 MHz 传输，4-bit 总线。
- GPT 分区：firmware、EFI、openvela 和 data。
- FAT 数据分区以读写方式挂载到 `/data`，支持 UTF-8 长文件名。
- 用户 LED、watchdog、THS 温度传感器、I2C2/I2C7、SPI1 和风扇 PWM。
- 测试板没有 UFS，并且 UFS 缺失被设计为非阻塞状态。

## 已验证的 FCU760K Wi-Fi 与服务

- 模组：FCU760K / AIC8800D80-U02，通过专用 USB1 高速链路连接。
- BootROM 标识 `a69c:8d80`，运行时标识 `a69c:8d81`。
- 固件上传、补丁配置、重新枚举、LMAC/RF/ME/法规域设置和 station VIF。
- 2.4 GHz 与 5 GHz 主动扫描、WPA2-PSK/CCMP、DHCP、ARP、DNS、TCP/UDP。
- 原生 `wifi` 命令、记忆网络重连和可选的开机自启动。
- SSH/SCP、FTP、curl/wget/HTTPS、NTP 和 iperf。
- 板载 TRNG 为 `/dev/random` 和 `/dev/urandom` 提供 TLS/SSH 随机数。
- 观测到的吞吐基线：TCP 4.13 Mbit/s；UDP 10.8 Mbit/s，单次 30 秒测试丢包
  5.7%。这只是功能基线，不是峰值性能声明。

## 已验证的 VIP2 NPU

- 电源/时钟/复位、IRQ、DMA 内存池、MMU 页表、缓存维护和同步任务 ABI。
- 硬件 CID `0x1000003b`；运行时设备 `/dev/npu0`。
- LeNet 输出 CRC32 `d50f5d79`，字节前缀
  `003c000000000000000000000000000000000000`，与 Linux VIPLite 黄金结果一致。
- YOLOv5s 三输出黄金回归通过。
- YOLOv8n PCQ 六输出黄金回归和动态 640×640 RGB 输入通过。dog 输入有检测结果，
  black 输入无检测结果，证明结果来自给定输入而不是回放固定输出。
- 当前交换路径为：获得授权的 Linux VIPLite prepare/trace → 已核对的 A7PM 包 →
  openvela 执行器。openvela 尚未实现通用专有 NBG 链接。

## 进行中的 USB UVC 摄像头

当前源码包含 v65 诊断实现。可以看到 Type-C 状态、USB2 PHY 和 xHCI MMIO，
但摄像头尚未完成枚举。证据表明，复位 xHCI/DWC3 会破坏厂商固件完成的
Cadence Combo PHY/PIPE 交接，之后 HCRST/CNR 恢复超时。

下一实现检查点：

1. 保留当前可用的固件交接状态；
2. 不再复位 DWC3 或 xHCI；
3. 建立 DMA32 DCBAA、命令环、事件环和中断器状态；
4. 启动控制器并发出 Enable Slot / Address Device；
5. 枚举描述符，实现 UVC PROBE/COMMIT 并采集帧；
6. 转换/缩放为 RGB640，送入现有 YOLOv8 动态输入并输出框、类别和分数。

## 尚未完成

- Bluetooth HCI 和协议配置文件；
- Imagination BXM GPU 运行时；
- UFS 主机/存储和 UFS 优先启动；
- MIPI CSI IMX214 驱动；
- SMP/DVFS/电源管理的生产级加固；
- openvela 内通用 NBG 编译器/链接器。

这些项目有意不计入已完成特性声明。

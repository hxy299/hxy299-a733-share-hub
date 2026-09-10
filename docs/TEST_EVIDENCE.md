# 真实开发板测试证据

开发板：瑞莎 Cubie A7Z，全志 A733，4 GiB 内存，microSD 启动，无 UFS。

## 启动与操作系统

观察到的成功启动边界：

```text
- Boot from EL1
- Boot to C runtime for OS Initialize
NuttShell (NSH)
nsh> uname -a
NuttX 0.0.0 ... arm64 cubie-a7z
```

`free` 报告约 4.27 GB 的托管内存；procfs 已挂载；`ps` 显示空闲线程、
高优先级工作队列和 NSH 任务。

## SD/GPT/FAT

修复 SDMMC 多块传输后，以下读取均无 I/O 错误：

```text
dd if=/dev/mmcsd0 of=/dev/null bs=512 count=1
dd if=/dev/mmcsd0 of=/dev/null bs=16384 count=64
dd if=/dev/mmcsd0 of=/dev/null bs=65536 count=16
```

开发板日志注册了 GPT 条目，并将 `/dev/a7z-data` 以读写方式挂载到 `/data`。

## 温度与开发板设备

```text
sensortest -n 3 temp0
temp0: ... value:34.47
temp0: ... value:34.47
temp0: ... value:34.47
```

已验证的设备节点包括 `/dev/userled`、`/dev/watchdog0`、
`/dev/uorb/sensor_temp0`、`/dev/i2c2`、`/dev/i2c7`、`/dev/spi1`、
`/dev/pwm0`、`/dev/mmcsd0`、`/dev/a733-wifi` 和 `/dev/npu0`。

## Wi-Fi 与 IPv4

FCU760K 已完成 VID:PID 从 `a69c:8d80` 到 `a69c:8d81` 的固件切换。扫描到
2.4 GHz BSS 和 5745 MHz 5 GHz BSS。开发板通过 WPA2 关联，并在测试网络中
由 DHCP 获取 `192.168.31.27/24`。网络名称和密码不在本仓库中。

网关 ping、互联网 IPv4 ping、DNS 查询、TCP、UDP、wget/curl、SSH 和 FTP
功能测试均通过。一次有代表性的本地 iperf 测试结果：

- TCP：30.25 秒传输 14.9 MiB，4.13 Mbit/s；
- UDP：30.15 秒传输 38.9 MiB，10.8 Mbit/s，丢包 5.7%。

FTP 测试覆盖小文件、大文件，以及本地路径中带空格和中文字符的 PDF。SSH
测试从 Windows 发起并连接到远端 NSH，覆盖主机密钥、ARP 和 socket 生命周期
修复后的行为。

## VIP2 模型执行

### LeNet

Linux VIPLite 参考结果：

```text
hardware CID=1000003b
profile inference=164..178 us
output[0] raw-crc32=d50f5d79
```

openvela `/dev/npu0` 产生相同 CRC 和 20 字节输出：

```text
003c000000000000000000000000000000000000
```

### YOLOv5s

使用确定性全零输入的官方 A733 模型，在 Linux 与 openvela 上均得到三个相同
的输出 CRC：

```text
53127e9c  ddb5675e  6e01633e
```

### YOLOv8n PCQ

Linux VIPLite 参考结果报告六路输出推理耗时 12,604 us。六个黄金 CRC32 为：

```text
55d0becd ead06611 ba209c57 2637657c 9e7f743a 2eb4d5ce
```

openvela 与内置 dog 输入的六路结果全部一致。动态提供一份 dog RGB 输入时
再次一致。使用黑色 640×640 RGB 输入时，输出 CRC 改变且解码得到零个检测框，
同时 MMU/IRQ/guard 检查保持干净。

## 当前 UVC 失败边界

最新诊断可以读取有效的 xHCI 能力寄存器块，但在破坏厂商 Combo PHY 交接后，
xHCI HCRST 无法清零；随后 DWC3/app 寄存器读取为零。这里记录的是正在处理的
驱动问题，不应宣称摄像头已经成功。

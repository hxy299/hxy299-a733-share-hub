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

## NPU ABI v4 构建检查点

2026-09-11 在当前 WSL openvela 环境中完成全量 AArch64 CMake 构建。驱动中的
`A733_NPUIOC_RUN_A7PM` 与原生 `npu` 应用均完成编译和最终链接，`System.map`
包含 `npu_main`。本次待刷写镜像为：

```text
cmake_out/cubie-a7z_nsh_npu/nuttx.bin
size:   1586024 bytes
sha256: bfc3d482d579f6c633af81ab5e002bdb9bcaa324b259719c155223ca396893be
```

首次配置时显式使用 `/usr/bin/python3`；WSL 的全局 `python` 仍保持原状。板端
回归命令和期望值见本次交付说明，未刷写前不把此 ABI 标记为硬件验证通过。

## 当前 UVC 失败边界

最新诊断可以读取有效的 xHCI 能力寄存器块，但在破坏厂商 Combo PHY 交接后，
xHCI HCRST 无法清零；随后 DWC3/app 寄存器读取为零。这里记录的是正在处理的
驱动问题，不应宣称摄像头已经成功。

后续 v113 实机仍未看到稳定端口连接；v114 加入 Combo PHY 表和最小 xHCI ring，
已随 v115 完整构建，但尚未上板。USB 麦克风和摄像头没有 VID/PID/描述符证据。

## 八核 SMP 与本地 LLM

板端 `cpucheck` 逐核确认 CPU0..5 为 Cortex-A55、CPU6..7 为 Cortex-A76，亲和
掩码覆盖 8 核。Wi-Fi、ping、NPU 和 NSH 在 SMP 镜像上继续工作。

Qwen2.5-1.5B Q4_K_M 模型完成 tokenizer、28 层前向、KV cache 和连续生成检查。
模型常驻后，默认 CPU2..7 六核工作池的单 token 前向检查点约 0.615 秒；板端
中文流式回答和有限多轮记忆已经观察到。该数值是特定检查点，不等同最终 tok/s，
也没有替代外部参考精度和长时间热稳定测试。

## 官方 Agent 与桌宠文字链路

实机执行 `aipet init` 后，DeepSeek 配置使用板级密钥加密保存，Agent 自启动。
连续两次中文请求成功，首次约 7490 ms，复用连接后的故事请求约 1829 ms。此前
第二次请求的非法 Unicode JSON 问题已修复，后续连续对话通过。

桌宠快速规则、云端 Agent、回复/表情/动作解析已进入源码。UART4 TW-TTS 和
I2S0 诊断进入 v115 候选镜像，但因没有开发板尚无实际发声/时钟日志，不能列为
硬件通过。

## UART4 TTS v115 失败与 v116 修复

v115 实机确认 `/dev/a733-audio` 可读取、官方 Agent 云端对话成功，但
`aipet tts test` 和 Agent 自动播报均返回 `-22`，且发送帧计数保持为零。
这证明错误发生在 UART 帧发送之前。源码复核确认 NuttX termios 的 `B9600`
是 `c_cflag` 中的编码选择值，而 `c_speed` 保存数值波特率；原驱动混用了
两者，使 `TCSETS` 稳定返回 `EINVAL`。

v116 已修正该 ABI 使用方式，并增加 `node=present|missing` 与
`stage=validate|open|termios|parameters|text|complete` 诊断。主机 TW-TTS 协议
测试、桌宠完整路由测试和 AArch64 全目标构建均通过。候选镜像校验如下：

```text
image:  openvela-a733-cubie-a7z-sd-uart-termios-v116-candidate.img
bytes:  2147483648
sha256: e03cb1bc3e9a9795eb2dd45530cba00c1357b4b1980f0097c567fd108bad35f9
kernel bytes:  1920592
kernel sha256: d55f12403d7c55a09b9bc395e2514e8dade0a8c2803e1c089322d435e50dd65e
```

`e2fsck -fn`、`sgdisk -v` 以及从镜像重新提取内核后的 SHA-256 比较均通过。
v116 实机得到 `node=present`，证明设备注册和 termios 修复有效；发送测试进入
`stage=parameters` 后返回 `-110`，`frames=0`，说明首个参数字节等待 UART
FIFO 可写超时。该结果取代“等待 v116 板测”的旧状态。

## UART4 TTS v117 时钟/复位修复候选

v117 保持官方 BSP 的 UART4 基址 `0x02504000`、BGR `CCU+0x0e10` 和
PJ24/PJ25 function 4，显式把 APB-UART `CCU+0x0538` 设为 24 MHz/1，随后
依次拉低复位、打开 UART4 门控、释放复位并执行 MMIO 读回/延时。新增只读
`/dev/a733-uart4`，可报告 APB 时钟、BGR、引脚复用、LCR、LSR 和 USR。

```text
image:  openvela-a733-cubie-a7z-sd-uart-hw-v117-candidate.img
bytes:  2147483648
sha256: 17658c8327cb6e015640212ba5bfdb3222779f8089a332a09bd82dff06fd54fe
kernel bytes:  1920592
kernel sha256: 418fe8b6a84479289f29029097414c45cb3b629db73c957cd51f9c27e7b1886d
```

AArch64 全目标编译/链接、`e2fsck -fn`、`sgdisk -v` 和镜像内核回读哈希均
通过。仍须实机取得 `thre=1` 或 `tfnf=1`、`stage=complete`、帧计数递增和
实际发声，才可标记 UART TTS 硬件通过。

## v67 WEXT/WAPI 构建与镜像校验

2026-09-14 完成全量构建和两次增量构建，以下目标均编译并链接成功：

```text
vendor/allwinnertech/chips/a733/a733_wifi_usb.c
apps/wireless/wapi
apps/system/a733wifi
```

构建配置包含 `CONFIG_NETDEV_WIRELESS_IOCTL=y`、`CONFIG_WIRELESS_WAPI=y` 和
`CONFIG_WIRELESS_WAPI_CMDTOOL=y`。最终内核：

```text
size:   1615080 bytes
sha256: 3f602f2cb25d3a0bf188e93dd93dbae9cdc33e1e50ca3f66d7c9809d0504434f
```

候选镜像：

```text
openvela-a733-cubie-a7z-sd-openvela-wapi-v67-candidate.img
size:   2147483648 bytes
sha256: 51934ef1f1bfc309970d507b8f970d5d4f77628a4fb35426a603c7df867313fe
```

独立校验从镜像重新提取 `/boot/openvela/a733/Image`，其大小和 SHA-256 与构建
产物完全相同；`e2fsck -fn` 和 `sgdisk -v` 均通过。构建脚本的 trap 也已恢复
临时复制进官方环境的源码。WAPI 的真实扫描、连接和断开仍须刷写后验证。

校验过程中发现并修复一个打包器问题：向已有镜像回写完整 ext4 分区时不能使用
`dd conv=sparse`，否则新分区中的零块不会覆盖基底镜像的旧字节。两个相关打包
脚本现已使用完整 `conv=notrunc` 回写，并在写入前逐字节比较嵌入内核。

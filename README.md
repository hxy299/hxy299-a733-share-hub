# Dogking A733：面向端侧 AI 的 openvela 新硬件适配

## 一、作品简介

本项目将 openvela/NuttX 原生适配到瑞莎 Radxa Cubie A7Z 开发板。该板搭载
Allwinner A733（2× Cortex-A76 + 6× Cortex-A55）、4 GiB LPDDR4/4X 和
3 TOPS VIP2 NPU。本项目不在 Linux 用户空间模拟 openvela，而是由板载
Boot0/SCP/BL31/U-Boot 直接引导 ARM64 openvela 内核进入 EL1。

当前已经完成可交互 NSH、4 GiB 内存、microSD/GPT/FAT、基础板级外设、
FCU760K 双频 Wi-Fi、WPA2/DHCP/IPv4、SSH/FTP/curl/wget，以及 VIP2 NPU 上
LeNet、YOLOv5s、YOLOv8n 的真实硬件推理。外接 USB UVC 摄像头链路处于开发中，
已定位到 Cadence Combo PHY/xHCI 固件交接后的复位恢复问题，尚不把摄像头采集
声明为完成。

作品面向离线 AI 助手和视觉识别终端：先把 openvela、联网、文件服务和 NPU
执行环境建立在 A733 上，再打通摄像头输入、YOLOv8 前后处理和结果输出。

## 二、选题方向

主赛道：**新硬件适配**。

扩展方向：**AI 硬件产品创新**。A733 VIP2 NPU 已在 openvela 下执行真实模型，
YOLOv8n 六输出和动态输入均已通过真机验证，而不是固定输出回放。

## 三、主要成果

| 子系统 | 状态 | 真机证据摘要 |
| --- | --- | --- |
| ARM64 启动 | 完成 | U-Boot `booti` → BL31 → EL1 → NSH |
| 控制台 | 完成 | UART0 115200 8N1，输入、termios、Ctrl+C |
| 内存/系统 | 完成 | 4 GiB split-region heap、procfs、`free/ps/uptime` |
| microSD | 完成 | SDMMC0 PIO，12 MHz/4-bit，多块读取 |
| 文件系统 | 完成 | GPT 分区节点，FAT 数据分区读写挂载到 `/data` |
| 板级外设 | 完成 | LED、WDT、THS、I2C2/I2C7、SPI1、风扇 PWM |
| Wi-Fi | 完成 | FCU760K 固件、2.4/5 GHz 扫描、WPA2、DHCP、DNS |
| 网络服务 | 完成 | SSH、SCP、FTP、curl、wget、NTP、iperf、可选自启动 |
| NPU | 完成基线 | VIP2 ABI、MMU/DMA/IRQ，LeNet/YOLOv5/YOLOv8 真机运行 |
| UVC 摄像头 | 进行中 | Type-C/PHY/xHCI 检查点完成，设备枚举尚未完成 |
| Bluetooth/GPU/UFS | 待完成 | 不计入当前完成项 |

详细状态、测试值与限制见 [实现状态](docs/IMPLEMENTATION_STATUS.md) 和
[测试证据](docs/TEST_EVIDENCE.md)。

## 四、仓库结构

```text
contest2026_274_Dogking/
├── board/a733-cubie-a7z/
│   ├── openvela-overlay/
│   │   ├── vendor/allwinnertech/chips/a733/        # A733 芯片与驱动
│   │   ├── vendor/allwinnertech/boards/a733/       # Cubie A7Z 板级代码
│   │   ├── apps/system/a733wifi/                    # Wi-Fi 管理命令
│   │   ├── apps/system/a733services/                # 自启动服务管理
│   │   ├── apps/system/a733ftpd/                    # FTP 服务封装
│   │   └── apps、nuttx 的必要兼容文件                # 逐文件 manifest 映射
│   └── README.md                                    # 板级说明
├── docs/
│   ├── IMPLEMENTATION_STATUS.md                     # 功能矩阵和边界
│   ├── TEST_EVIDENCE.md                             # 真机验收证据
│   ├── ARTIFACTS.md                                 # 镜像/模型校验与获取规则
│   └── UPSTREAM_PLAN.md                             # 公共仓拆分计划
├── tools/build-a733.sh                              # WSL/Linux 构建入口
├── logs/                                            # 官方工具导出的真实 AI 日志
├── contest2026_274_Dogking.xml                      # 仓库 manifest 与 linkfile
└── openvela.xml                                     # 大赛官方基线 manifest
```

模板中的 hello app、quickapp 和示例日志已删除。本作品只使用板级适配形态。

## 五、获取完整 openvela 工作区

推荐 Ubuntu 22.04/24.04 或 Windows 11 + WSL2 Ubuntu。以下操作均在 Linux/WSL
终端执行；不要把 Linux 构建工具直接换成 Windows 工具。

```bash
mkdir -p ~/openvela-contest
cd ~/openvela-contest

repo init \
  -u https://github.com/open-vela/contest2026_274_Dogking \
  -b dev-ai-contest-2026 \
  -m contest2026_274_Dogking.xml
repo sync -c -j8
```

同步后，比赛仓库在 `contest2026_274_Dogking/`，其代码由 manifest 的
`linkfile` 自动映射到外层 `vendor/`、`apps/` 和 `nuttx/`。开发者只应提交本
比赛仓库；公共仓修改的长期合入方式见 `docs/UPSTREAM_PLAN.md`。

## 六、构建

在完整工作区根目录执行：

```bash
cd ~/openvela-contest
bash contest2026_274_Dogking/tools/build-a733.sh
```

等价的底层命令为：

```bash
./nuttx/tools/build.sh \
  vendor/allwinnertech/boards/a733/cubie-a7z/configs/nsh \
  --cmake \
  -b cmake_out/cubie-a7z_nsh \
  -j8
```

增量构建：

```bash
bash contest2026_274_Dogking/tools/build-a733.sh --incremental
```

主要产物：

```text
cmake_out/cubie-a7z_nsh/nuttx.bin
cmake_out/cubie-a7z_nsh/nuttx
cmake_out/cubie-a7z_nsh/System.map
```

`nuttx.bin` 带 Linux ARM64 Image header，必须由 U-Boot `booti` 启动；不能用
`go 0x40200000` 进入 AArch64 内核。

## 七、SD 卡启动与部署

当前开发板是 4 GiB、未安装 UFS 的 Cubie A7Z，优先从 microSD 启动。Boot0、
SCP、BL31 和 U-Boot 复用瑞莎官方可工作的启动固件，openvela 已不依赖 Debian
rootfs。比赛仓库不提交 2 GiB/4 GiB 镜像，避免 GitHub 大文件限制；镜像校验值
和发布规则见 `docs/ARTIFACTS.md`。

已经具备 A733 启动固件的 SD 卡可直接替换 openvela 分区中的：

```text
/boot/openvela/a733/Image
```

替换内容为构建出的 `nuttx.bin`。extlinux 条目参考：

```text
LABEL openvela A733
  KERNEL /boot/openvela/a733/Image
  FDT /boot/dtb/sun60i-a733-cubie-a7z.dtb
```

串口：115200 8N1，无硬件流控。预期最终出现：

```text
NuttShell (NSH)
nsh>
```

## 八、上板验收

基础系统：

```text
uname -a
free
ps
mount
ls /dev
cat /dev/a733-hw
sensortest -n 3 temp0
```

存储：

```text
dd if=/dev/mmcsd0 of=/dev/null bs=512 count=1
dd if=/dev/mmcsd0 of=/dev/null bs=16384 count=64
ls /data
```

Wi-Fi 与网络（密码只在板端交互输入，禁止写入源码）：

```text
wifi scan
wifi connect <SSID>
ifconfig
ping -c 4 <LAN_GATEWAY>
nslookup example.com
curl http://example.com/
```

NPU（A7PM 文件需按 `docs/ARTIFACTS.md` 合法准备并复制到 `/data/npu`）：

```text
cat /dev/npu0
echo selftest > /dev/npu0
echo apitest > /dev/npu0
echo prepared=/data/npu/lenet.a7pm > /dev/npu0
cat /dev/npu0
```

USB 摄像头当前只能用于诊断，不能作为已完成验收项：

```text
cat /dev/a733-uvc
echo probe > /dev/a733-uvc
dmesg
```

## 九、AI Coding 使用说明

本适配使用 AI 完成资料归纳、启动阶段拆解、寄存器对照、驱动骨架、故障假设、
构建脚本、真机日志分析、回归清单和文档整理。所有硬件结论都以串口日志和真机
命令结果验证；“能编译”不被当作“适配完成”。

AI 辅助的典型闭环：

1. 从原理图、A733 BSP、Linux/U-Boot 和手册中提取可验证假设；
2. 一次只增加一个硬件检查点，并为轮询设置超时；
3. WSL 构建、生成新版本镜像，不覆盖已验证恢复点；
4. 用户在真机执行命令并提供完整串口日志；
5. AI 根据寄存器、错误码和前后版本差异定位；
6. 通过回归后再合并下一批功能。

官方归集工具导出的原始会话位于 `logs/`。这些 JSONL 不应手工修改；不提交的
会话应整段删除，而不是删改单条事件。

## 十、安全、许可与隐私

- 项目源码使用 Apache License 2.0；第三方代码继续遵循其原许可证。
- 不提交 Wi-Fi 密码、SSH/FTP 密码、私钥、API key、个人路径或账号令牌。
- 不重新分发许可不明的 Allwinner VIPLite/NPU 编译器、闭源库或固件镜像。
- 模型和大镜像使用校验和、来源和可重复生成步骤描述，不直接塞入 Git 历史。
- 所有未完成子系统均在文档中明确标注，不以检查点冒充完整功能。

## 十一、已知限制和后续计划

1. UVC 摄像头尚未枚举；优先完成保留固件 handoff 的 xHCI ring 初始化，再打通
   UVC → RGB640 → YOLOv8 → bbox 输出。
2. Bluetooth HCI、Imagination GPU、UFS 尚未适配。
3. Wi-Fi 已可用但仍需长稳、断线重连、并发吞吐和 DFS 压测。
4. NPU 当前采用官方 Linux VIPLite golden trace → A7PM → openvela 执行路线；
   通用 NBG 编译/链接仍依赖官方授权工具。
5. 公共 `apps`/`nuttx` 兼容修改需要按官方流程拆分并提交对应仓库的
   `dev-ai-contest-2026` PR。

## 十二、官方大赛资料

- [大赛总览](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/contest_overview.md)
- [参赛代码提交指南](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/code_submission_guide.md)
- [AI Coding 日志手册](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md)
- [新硬件适配指引](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/hardware_porting/hardware_porting_track_guide.md)

## 许可证

Apache-2.0。详见 [LICENSE](LICENSE)。

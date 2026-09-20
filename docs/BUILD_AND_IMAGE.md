# A733 完整构建、镜像生成与校验

本文给出从公开协作仓库复现当前 openvela A733 固件的方法。命令在 Ubuntu
22.04/24.04 或 WSL2 Ubuntu 中执行。Windows PowerShell 只用于 Git 和文件管理，
不要直接代替 Linux 构建环境。

## 1. 获取工作区

```bash
mkdir -p ~/openvela-contest
cd ~/openvela-contest
repo init \
  -u https://github.com/hxy299/hxy299-a733-share-hub.git \
  -b a733-cubie-a7z-share \
  -m contest2026_274_Dogking.xml
repo sync -c -j8
```

manifest 将比赛仓库检出为 `contest2026_274_Dogking/`，并把板级源码、应用和
必要兼容文件映射到 openvela 工作树。`packages_ai_agent` 等公共仓修改以仓内
patch 保存，构建脚本临时应用并在退出时恢复，避免污染公共仓源码。

## 2. 完整构建

```bash
cd ~/openvela-contest
bash contest2026_274_Dogking/tools/build-a733.sh
```

可选参数和环境变量：

```bash
bash contest2026_274_Dogking/tools/build-a733.sh --incremental
JOBS=12 bash contest2026_274_Dogking/tools/build-a733.sh

OPENVELA_WORKSPACE=/path/to/openvela \
A733_BUILD_DIR=/path/to/openvela/cmake_out/cubie-a7z_nsh \
bash contest2026_274_Dogking/tools/build-a733.sh
```

脚本会检查 openvela-first 架构边界，临时应用公共 Agent、readline 和 ARM64
兼容 patch，完成 Cubie A7Z 构建，核对 SMP/WAPI 配置并输出 SHA-256。无论成功、
失败或中断，都会恢复临时修改的公共仓文件。

主要产物：

```text
cmake_out/cubie-a7z_nsh/nuttx.bin
cmake_out/cubie-a7z_nsh/nuttx
cmake_out/cubie-a7z_nsh/System.map
```

`nuttx.bin` 带 ARM64 Linux Image header，必须通过 U-Boot `booti` 启动。

## 3. 生成可烧写 SD 镜像

本仓不重新分发 Boot0/SCP/BL31/U-Boot。以一份已经验证能启动 Cubie A7Z 的
基础 SD 镜像为输入，只替换 openvela 分区中的内核：

```bash
cd ~/openvela-contest
BASE=/path/to/known-good-a733-base.img
OUT=/path/to/openvela-a733-current-candidate.img
KERNEL=cmake_out/cubie-a7z_nsh/nuttx.bin

bash contest2026_274_Dogking/tools/package-a733-kernel-image.sh \
  "$BASE" "$OUT" "$KERNEL"
```

脚本拒绝覆盖基础镜像或已存在的输出文件，提取 GPT 第 3 分区，通过 `debugfs`
替换 `/boot/openvela/a733/Image`，回读比较内核，再把完整 ext4 分区写入新镜像。

## 4. 独立校验

```bash
KERNEL_SHA=$(sha256sum cmake_out/cubie-a7z_nsh/nuttx.bin | cut -d' ' -f1)
bash contest2026_274_Dogking/tools/verify-a733-image.sh \
  /path/to/openvela-a733-current-candidate.img "$KERNEL_SHA"
```

校验包括 `e2fsck -fn`、回读内嵌内核、`sgdisk -v`、镜像大小和 SHA-256。
依赖通常由 `coreutils`、`e2fsprogs` 和 `gdisk` 提供。

## 5. 发布边界

- `.img`、ELF、模型、A7PM、构建日志和本地恢复 bundle 不进入 Git。
- 可公开分发的最终镜像应上传 GitHub Release，并在 `docs/ARTIFACTS.md` 写明
  来源、大小、SHA-256 和实机验收状态。
- v120 是当前最新可恢复候选镜像；它在已通过 UART4 TTS、Wi-Fi、官方 Agent
  和本地 Qwen 接口的 v119 基线上加入 ST7735/LVGL。v120 已完成构建和镜像校验，
  但显示硬件仍等待实机测试，不应标记为正式发布版。
- 烧录会覆盖目标 TF 卡。先备份 `/data/models`、Wi-Fi、SSH/FTP、Agent 密钥
  和私人配置。凭据禁止进入 Git 历史或公开镜像。

# 外部制品与完整性校验

大型镜像、NPU 模型、厂商固件和专有工具不存放在 Git 历史中。这样既可
避免 GitHub 体积限制，也不会重新分发受许可证限制的材料。

## 最新可恢复候选镜像

```text
名称：openvela-a733-cubie-a7z-sd-audio-uart-v115-candidate.img
SHA-256：cf25ac887087e5bbad1863258225d2060c3c875706b84686d1ef8cdc6b01c834
```

镜像内嵌内核：

```text
SHA-256：2d3af39ada75f72e42f127867637c2643142ec2d949c0117b1c62d8d5df80d08
```

对应源码提交为 `002a310`，恢复 bundle SHA-256 为
`c09167d64ff263d78684012f6c1eca892f2293b019af09a4e1b299940bb266d4`。
完整交叉构建、ext4、GPT 和内嵌内核一致性校验已经通过。由于当前没有开发板，
UART4 TW-TTS 实际发声和 I2S0 诊断仍待验证，因此 v115 不是正式发布版。

镜像由已验证可启动的 A733 基础镜像复制后，仅替换第 3 分区中的 openvela 内核；
基础镜像不会被修改。可重复命令见 `docs/BUILD_AND_IMAGE.md`。

最终比赛发布版本必须重新构建、实机回归并验证镜像，将其作为 GitHub Release 资源，
同时在本文件中补充下载地址、大小和 SHA-256。

## 开发期间使用的 NPU 预处理包

以下信息可供拥有官方模型和 VIPLite 工具合法访问权限的评估者核对确切
输入：

| 制品 | 大小 | SHA-256 |
| --- | ---: | --- |
| `lenet.a7pm` | 441856 | `1e65fc966d5aabbda46f573e16c41c63e6ca3019d241feea12190b1cb38aa027` |
| `yolov5s-zero.a7pm` | 11239104 | `3feb08593d743bb5ca239110013954fe08ad8a864c81fd719b65b2630dbc4472` |
| `yolov8n-pcq-v3.a7pm` | 8017024 | `3c2891ef77ce9a44dd02cd32bdd7db5ec92d14185014e6de8db5803a6cbecce2` |
| dog RGB 输入 | 1228800 | `07a1654c992b1a2e38ccc595a571994889134df9594eef09b5a85d70f4820c60` |
| black RGB 输入 | 1228800 | `3630e065eb7b4540fbab11dbfd2619e8500f211b9c404380a1867fdc44b77c0c` |

A7PM 包含通过官方 Linux VIPLite prepare/run 流程获取并核对过的 VIP 虚拟
地址布局。它们不能替代或解码 Allwinner 专有的 `.nb` 编译器/链接器。

## 仅运行时使用的数据

以下内容应放在开发板数据分区中，不应进入源代码管理：

- 从官方 BSP 获取的 FCU760K D80-U02 固件；
- HTTPS CA 证书；
- 生成的 SSH 主机密钥；
- 本地 Wi-Fi 凭据和服务配置；
- 许可条款允许在本地使用的 A7PM/模型文件；
- 用户数据、采集帧和推理输出。

构建本仓库不需要密码、私钥或 API 令牌。

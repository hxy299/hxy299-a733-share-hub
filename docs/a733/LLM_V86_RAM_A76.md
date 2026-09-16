# LLM v86：RAM 模型与单 A76 前向对照

前置恢复点：`a733-smp-8cpu-a76-v85-basic-verified`。不修改官方公共
源码，不改变已验证的八核启动配置；所有新增实现位于比赛仓库 overlay。

新增命令 `aipetllm forwardfast model.gguf token-id`。当前任务绑定 CPU6，
检查 MIDR part=d0b 后，将模型以 256 KiB 分块读入完整 RAM 缓冲区，
通过官方工作区 libc 的只读 `fmemopen` 复用已有 GGUF/量化/前向代码。
分别输出模型载入和计算时间。正常退出及受控错误时释放模型缓冲区并
恢复原亲和掩码。普通文件最大 1536 MiB；不足的连续内存会明确报错。

这是单次命令内的模型驻留：结束后释放，不是跨命令持久缓存，也不是
多 token 生成。仅一个 A76 计算线程，CPU7 和六个 A55 尚未承担矩阵
并行计算。原 `forward1` 行为保留作为基准。没有调整 CPU PLL/频率。
不要同时运行多个 forwardfast，避免多个约 1.1 GB 的模型副本。

## 实机检查

```text
aipetllm cpucheck
free
time "aipetllm forwardfast /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707"
free
```

首次实机检查应等待正常结束，检查结束后内存恢复。预期数值沿用已有
计算路径：final-state-crc=17a04f7d、logits-crc=7fdfee67、argmax=6233。
如不同，保存逐层 CRC，不得仅凭成功返回断言数值正确。CRC 也仅是
回归证据，尚未替代与独立参考实现的误差验证。

旧单 A55+SD 流式 forward1 耗时 473.6016 秒；新的加载/计算数据必须
实测，不能把模型加载耗时直接当作每 token 推理速度。

后续：保持模型和解析目录的生命周期、矩阵行按两 A76/异构核心分工、
多 token prefill/持续 KV cache/采样循环、量化 NEON 回归、长时间温度
和网络/NPU 并发稳定性。当前仍只是单 token 前向，不能进行完整对话。

# v94：合并预填充、因果 KV 和连续生成

基于 v93 双 token 实机验证状态；保留 SMP 页对齐栈修复、RAM 模型缓存、持久线程池和默认 CPU2–7 策略。系统其他任务的调度策略不变。

## 合并能力

- `generateids MODEL IDS COUNT [CORES]`：输入 1–64 个十进制 token ID，逗号分隔。
- `generate MODEL "raw text" COUNT [CORES]`：复用现有 Qwen2 BPE，提示词最多 4096 字节、64 token。
- COUNT 为 0–64：0 仅预填充；大于 0 贪心 argmax 连续生成，遇 GGUF eos_token_id 停止。
- 每层每位置的 K/V 在本次命令中保存，后续位置不重算历史；最大处理位置数 127。Qwen2 使用 NeoX split-half RoPE。
- 正常结束后解码生成 token 为文本；目前不是实时文本流。模型和线程池跨命令保留，KV 每次命令结束释放。
- 可选 CORES 如 `6,7`；缺省 `2,3,4,5,6,7`。输入数字边界、重复核心和非有限 logits 检查。

这是原始文本续写入口，不是已经完成的聊天助手。尚未加入 ChatML 对话模板、跨命令对话 KV、采样参数、ASR/TTS 或 NPU LLM 加速。BPE 的完整 Unicode 分类和特殊 token 识别仍需审计；不要手工把 ChatML 标记拼成普通文本当成特殊 token。

## 同一次烧录的测试清单

命令均在一行执行。第一次加载模型约 175 秒；随后复用 RAM。

```sh
aipetllm generateids /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707 1
aipetllm generateids /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707,1879 4
aipetllm generateids /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707,1879 4
aipetllm generate /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf "Hello world" 4
aipetllm cacheinfo
aipetllm unload
aipetllm cacheinfo
free
```

单 token 第一预测应为 6233；双 token 第一预测应为 4894（v93 position=1）。相同序列两次应有相同生成 ID、层 CRC，created 不随矩阵次数增长；文本入口应与 9707,1879 数字入口相符。卸载后 workers=0。也应测试 COUNT=65、空 CSV、尾逗号、超词表 ID、重复核心，确认安全拒绝。

## 验证边界和后续任务

编译和镜像检查不能替代板端执行。连续生成需实机验证，并与 Linux/llama.cpp 同 GGUF、同输入序列独立核对；重复一致不等于模型精度正确。v93 只有运行重复性验证，没有独立参考精度验证。EOS 提前结束和 64-token 边界需要额外测试。

任意 kill/Ctrl+C 的清理尚未审计，不宣称可随意中断。模型缓存互斥保护和关闭 pthread cancellation 仅覆盖已有正常调用路径。高负载网络并行、长时间生成、重复加载/卸载内存压力仍待测试。

保留 v93 镜像、源码标签和 `a733-v93-before-merged-generation.bundle` 回退。构建前保存源码提交/bundle；通过临时 overlay 构建并恢复官方文件。整卡烧写前备份 TF 卡模型、密钥和配置，生成镜像不含用户后来传入的 1.1 GB GGUF。

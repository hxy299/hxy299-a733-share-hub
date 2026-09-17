# v93：完整 28 层双 token 因果 KV 验证

基于已通过线程池/缓存回归的 v92，保留页对齐的 SMP 栈和原调度策略。

## 新增能力

`aipetllm forward2fast model.gguf token1 token2 [cores]`。

一次调用中按顺序处理两 token，每层保存两个位置的 K/V，第二个位置使用历史 K/V、因果 GQA、稳定 softmax，再执行输出投影、残差、SwiGLU 和 LM head。默认 cores=2,3,4,5,6,7；模型 RAM 缓存和持久线程池跨调用复用，KV 仅在本次调用期间有效，结束后释放。

Qwen2 RoPE 经本地 `llama.cpp-0.1.2/src/llama-model.cpp` 的 `llama_model_rope_type` 和 `src/models/qwen2.cpp` 核对，使用 NeoX split-half 布局。早期 `qkv/attn2` 诊断的 NORMAL 相邻成对 RoPE 尚未重新审计，旧位置一 CRC 不能作为真实 Qwen2 多 token 正确性的参考。

没有实现跨命令持久会话、任意长度 prefill、采样、连续生成或聊天封装。不是完整离线助手。

## 板端测试（逐行执行）

```text
time "aipetllm forwardfast /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707"
time "aipetllm forward2fast /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707 1879"
aipetllm cacheinfo
time "aipetllm forward2fast /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707 1879"
aipetllm cacheinfo
aipetllm unload
free
```

首 token 所有层和最终输出应与 v92 相同：final-state=17a04f7d、logits=7fdfee67、argmax=6233。双 token position=1 不应等同于独立 token=1879 的无历史前向，两次相同序列必须得到相同 CRC/argmax。保持相同 mask，created 不再增长；卸载后 workers=0。

双 token 每次矩阵计数预期新增 394（每 token 28*7+1），旧单 token 新增 141。first token 不重算历史，但本命令还未提供跨调用增量会话。

用户发回两个位置完整数值后，需要与 Linux/llama.cpp 同一 GGUF、同 token 序列、同 Q8_K 输入量化数值路径作参考核对。编译通过和重复一致不能替代模型正确性验证。

整卡烧录前备份模型、密钥及配置。源码恢复 bundle、ELF/Image/System.map 和日志独立存档；不覆盖 v92 恢复版本，不永久修改官方代码。

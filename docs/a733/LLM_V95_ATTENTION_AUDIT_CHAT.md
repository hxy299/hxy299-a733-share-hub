# v95：attention 对照诊断与单轮 ChatML

## 背景和证据边界

v94 实机已生成 8 token 并正常返回；相同 Hello world 的第二位置 argmax 与 v93 不同（4894→271），第一位置则完全一致。比较源码可见 KV 的分配、写入、读取均已统一改为 capacity 步长；尚无证据证明该布局损坏。泛化 attention 同时将两项显式表达式改成循环累加，浮点求值/指令融合可能改变结果，但**这还不是已证实的根因**，也不能认定 v93 就是精度正确的参考。

v95 在 position=1 保留 v93 的显式两项 context 表达式作为兼容基线，并在同一 Q/K/V、同一 softmax 上计算泛化表达式，报告 changed 和 maxabs；随后选用两项表达式。每层打印 Q/K/V/context CRC，便于判断偏差是否发生在输入投影之前。更长历史仍使用泛化累加。不把旧 CRC 恢复或两个表达式相等当作独立模型精度验收。

## 新的对话入口

```sh
aipetllm chat /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf "Hello" 16
```

单轮 ChatML：system=You are a helpful assistant.；随后 user 消息和 assistant 起始标记。im_start/im_end 从模型词表按字符串查找并直接写入 ID，不经普通 BPE。当前 Qwen2 词表为 151644/151645。普通角色、换行、正文仍复用现有 BPE，完整 Unicode 类别审计尚未完成。用户文本包含 `<|` 时拒绝，以免控制标记注入。最大模板加正文共 64 输入 token、4096 字节正文，最多生成 64 token。

chat 遇模型 EOS 或 im_end 停止，控制 token 不作为回答文本解码。只在最后一个提示位置计算 LM head，前面位置保留完整逐层 KV；这不改变后续 Transformer 的状态。chat 每位置仅打印第一/最后层 CRC，保留进度，减少串口压力。generate/generateids 保留完整诊断。最终文本仍在生成完成后一次性解码，非流式。

仍未实现跨命令对话 KV、多轮聊天、采样、Ctrl+C 安全取消、ASR/TTS。模型/线程池仍驻留，unload 释放模型和工作线程。不得关闭热保护或把用户报告的降频当作已经测量到的事实。

## 一次烧录回归顺序

```sh
aipetllm forward2fast /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707 1879
aipetllm generateids /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707,1879 4
aipetllm generateids /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707,1879 4
aipetllm chat /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf "Hello" 16
aipetllm cacheinfo
aipetllm unload
aipetllm cacheinfo
free
```

请保留 attention-audit 全部行、第二位置层 CRC 和 argmax。若显式表达式也不恢复 v93，要继续核对 Q/K/RoPE、模型一致性、编译求值和数据路径，不能直接宣布修复。独立 Linux/llama.cpp 同 GGUF 参考仍必要。chat 对话效果需实机验证；EOS、长度边界和重复压力另外测试。

v94 源码回退 bundle：上层目录 `a733-v94-before-attention-audit-chat.bundle`。所有实现保存在仓库 overlay；每次构建前提交和 bundle，临时覆盖官方环境后由脚本恢复；新镜像独立命名，不覆盖 v94/v93。整卡烧录前备份 TF 模型、密钥和配置。

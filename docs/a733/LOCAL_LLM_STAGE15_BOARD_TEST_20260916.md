# A733 本地大模型阶段 15 真机验证

测试平台：Cubie A7Z / A733，openvela 最小 SD 镜像 v81。

## 测试命令

    aipetllm forward1 /data/models/qwen2.5-1.5b-instruct-q4_k_m.gguf 9707

输入 token 9707 对应此前 tokenizer/decode 检查中的 Hello。

## 逐层 state CRC

| 层 | CRC | 层 | CRC |
|---:|:---:|---:|:---:|
| 0 | f35f4323 | 14 | 6b594e33 |
| 1 | 43750cf3 | 15 | bd2b9356 |
| 2 | 18d69f16 | 16 | 9c91d253 |
| 3 | aa231488 | 17 | 8a8199d9 |
| 4 | a209163e | 18 | 564ad6b6 |
| 5 | c82850bb | 19 | 8794d9c7 |
| 6 | dfd26c49 | 20 | 66b6f7e4 |
| 7 | 435d8044 | 21 | 665a5f69 |
| 8 | d86e8a55 | 22 | 60f67a8e |
| 9 | 835bb607 | 23 | ffaad38b |
| 10 | 94662d84 | 24 | 70e99d11 |
| 11 | c4c62b55 | 25 | 5f3d61e4 |
| 12 | a18160cd | 26 | 486fd0fa |
| 13 | 9a2fe25c | 27 | 451d1506 |

## 最终输出

- 最终归一化 state CRC：17a04f7d；
- 完整 logits CRC：7fdfee67；
- 词表大小：151936；
- argmax token：6233；
- argmax logit：10.4909735。

真机最终输出：

    Qwen2 28-layer single-token forward and LM head checkpoint passed; multi-token KV-cache generation pending.

结论：流式 GGUF 读取、28 层参数化执行、最终 RMSNorm、完整 LM Head 和 argmax 已在
A733/openvela 真机通过。上述逐层 CRC 作为后续多 token 执行器的单 token 回归黄金基线。

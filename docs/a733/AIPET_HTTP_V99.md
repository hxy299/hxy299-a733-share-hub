# v99 HTTP 响应接收修复与板测

## v98 实机证据

DeepSeek 配置读入，严格 TLS 握手成功。请求从 60.795 秒到 122.014 秒，官方引擎解析到 27 字节回复，但 61,219 ms 超过 Agent 60 秒看门狗；aipet 已在 30 秒时退出。
这证明鉴权配置至少已读入，HTTPS 和回复解析链路可用，但未完成成功呈现。旧日志没有首字节/消息边界时间，因此不能将全部等待确定归因于服务端或接收器。

## 修复

- chunked 按块长度推进，识别零块及完整 trailer 结束，不等待 keep-alive 连接关闭；不使用简单结束串搜索，避免误命中 payload。
- Content-Length 收满即返回；Transfer-Encoding 优先于 Content-Length。
- 错误块格式、过长响应、提前 EOF 或读取超时返回失败，不能把不完整回复交给 JSON 层当作成功。
- HTTP 响应头和正文共用 45 秒单调时钟预算，每次读取设置剩余 socket 时间。
- aipet 本地等待为 90 秒，Agent 60 秒单次调用看门狗保留。90 秒不是整个引擎的网络取消保证；多轮工具调用/外部 DNS/连接仍受各自机制控制。
- 已发送 POST 后的写入/接收失败不在连接池内静默重发，避免重复计费；服务端明确 429 的官方重试机制仍保留。
- 日志只记录 first-byte、HTTP status、framing、complete 和 encoded-bytes，不打印请求头、响应内容或 API key。
- 启用 FAT_LCNAMES 兼容 mtools 写入的短文件名大小写位；目录初始化改用配置 AGENT_DATA_DIR，技能目录不再硬编码 /data/agent。

`patches/ai-agent-http-completion.patch` 在构建中临时作用于官方源码；退出时由构建脚本恢复。解析器位于仓库 overlay 的 `apps/system/aipet/pet_http_framing.h`。

## 板测

1. 备份旧 TF 卡私人配置，烧录新镜像；新镜像不包含 API key。
2. `ls /data/ai_agent`、`ls /data/ai_agent/config` 应直接可访问。目录显示大小写可能因短文件名标志不同而变化，以小写路径能访问为准。
3. 联网、确认时间；FTP 上传私有 config.json 到 /data/models，再复制到 /data/ai_agent/config/config.json。
4. 只启动一次 `ai_agent &`，等待服务启动，执行 `aipet ask "请只回答：连接测试成功"`。
5. 记录完整回复及 HTTP first-byte/status/framing/complete 日志。预期不再在响应已完整时等待连接关闭。
6. 如 first-byte 自身很慢，进一步排查服务端、连接和网络；不能继续归因于 chunked 收尾。
7. 不要以断网后能收到旧请求回复认定取消成功；本地超时与取消仅阻止旧回复呈现。

当前仍为官方 Agent 文本集成候选，不是完整语音桌宠。

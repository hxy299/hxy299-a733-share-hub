# 官方提交规则核对清单

核对日期：2026-09-20。依据 openvela 官方 `dev-ai-contest-2026` 分支的《参赛
代码提交指南》和《AI Coding 日志归集与提交手册》。本文件区分“个人协作远端”
与“组委会正式提交”，避免把临时共享配置误提交为最终 manifest。

## 当前个人协作阶段

- 当前分支：`a733-cubie-a7z-share`。
- 当前推送目标：个人公开仓库 `hxy299/hxy299-a733-share-hub` 的 `share` 远端。
- manifest 和 README 的 `repo init` 命令暂时指向个人仓库，便于队友同步开发。
- 不向组委会 `origin` 推送；不创建正式比赛 PR。

## 提交就绪状态（2026-09-20）

作品提交相关的全部内容已在本地完成并提交到 Git，**只差一次 `git push`**。

```text
本地提交   e6f5c94  docs: prepare competition submission and integrate three-SoC ports
基于远端   34aa0a8  docs: follow renamed personal collaboration remote
推送关系   快进（fast-forward），普通 push 即可，无需 force
待推送     1 个提交 / 555 个文件 / +61,970 行
工作区     clean（无未提交改动）
```

### 网络阻塞说明

打包时本机**无法访问 github.com**（WSL 与 Windows 两侧均连接超时，
`Failed to connect to github.com port 443`）。因此提交已完成但**尚未推送**。

网络恢复后执行以下任一条即可完成上传：

```bash
# 方式 1：在 WSL 工作区推送个人协作远端
cd ~/openvela-contest/contest2026_274_Dogking
git push dogking-share a733-cubie-a7z-share

# 方式 2：若已可访问组委会仓库，改推组委会 origin
git remote add origin https://github.com/open-vela/contest2026_274_Dogking.git
git push origin a733-cubie-a7z-share
```

推送后请核对 GitHub 上确实出现：

```text
docs/SUBMISSION_TECHNICAL_REPORT.md
docs/rockchip/ROCKCHIP_RK3506_LYRA_ZERO.md
docs/rockchip/ROCKCHIP_RV1103_PICO_MINI.md
board/rockchip/                  （92 个文件）
skills/                          （3 个 Skill，18 个文件）
logs/hxy299/manifest.json
logs/hxy299/<date>/codex__*.jsonl （107 个会话文件）
```

### 推送前已通过的机械检查

```text
git diff --check                     clean（无空白/冲突标记问题）
bash -n tools/*.sh 与板级 tools/*.sh  全部 OK（17 个脚本）
git ls-files | grep -E '\.(img|gguf|a7pm|nb|bin|elf)$'   无命中
git ls-files | grep -E '\.(key|pem|p12|crt)$'            无命中
密钥模式扫描（api_key / BEGIN PRIVATE KEY / sk-）          logs/ 外无命中
官方日志校验 validate-log.py                             107 files / 23,933 events / ALL OK
```

## 正式提交前必须恢复

1. 将 manifest 项目恢复为组委会仓库 `open-vela/contest2026_274_Dogking`，分支
   `dev-ai-contest-2026`，并同步修改 README 的 `repo init` 示例。
2. 从个人分支创建面向组委会专属仓库的 PR；遵守保护分支，不强推覆盖历史。
3. 使用报名账号签署 CLA；若 `cla/signature` 未刷新，在 PR 评论 `/check-cla`。
4. 公共 `nuttx/apps/packages/vendor` 修改按功能拆分到对应公共仓 fork，并向
   `dev-ai-contest-2026` 提交 PR；比赛仓保留 overlay/patch 是当前可复现开发形态，
   不能替代获奖后的上游合入。

## 仓库内容检查

- [x] README 包含作品简介、赛道、三平台、目录、构建/运行、AI Coding 和限制。
- [x] `contest2026_274_Dogking.xml` 可映射板级和应用源码（含 A733 与 Rockchip）。
- [x] 源码只在比赛仓维护；构建脚本临时 patch 后会恢复公共工作区。
- [x] 镜像生成、校验和烧录边界有可重复文档。
- [x] 未完成的 USB、I2S、显示颜色、动作、视觉链路明确标记，不以编译代替实机通过。
- [x] `.img/.bin/.elf/.a7pm/.nb/.gguf`、本地归档和恢复 bundle 不进入 Git。
- [x] Wi-Fi、SSH/FTP、API Key、设备密钥和私人配置不进入 Git。
- [x] 技术报告已按官方模板撰写：`docs/SUBMISSION_TECHNICAL_REPORT.md`。
- [x] 三个自定义 Skill 已入仓：`skills/`。
- [x] Rockchip 两平台移植已入仓：`board/rockchip/` + `docs/rockchip/`。
- [x] A733 历史阶段文档已归档：`docs/a733/version-archive/`。
- [ ] 正式候选镜像完成最终实机回归并上传 GitHub Release。
- [ ] Release 地址、大小、内核与镜像 SHA-256 更新到 `docs/ARTIFACTS.md`。

## AI Coding 日志（已完成）

官方要求每位成员安装归集器，日志按本人 GitHub login 放入
`logs/<github_login>/`，提交真实导出的 `manifest.json` 与 JSONL，不得手工拼接或
修改事件。

本仓已完成归集：

```text
logs/hxy299/manifest.json        # schema_version 1.0
logs/hxy299/<YYYY-MM-DD>/codex__<session_id>.jsonl
```

- 会话数：**107**；事件数：**23,933**；时间跨度：2026-07-27 → 2026-09-14。
- 官方校验器结果：**107 files checked / 23,933 events checked / ALL OK**。

```bash
cd <专属仓>
python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/
```

**转换说明（重要）**：本仓日志来自 Codex rollout 导出树，不是 collector 钩子
自动写入的。转换脚本按官方 `schema/event.schema.json` 与
`schema/manifest.schema.json` **重塑并标注**事件，遵守三条原则：

1. **不虚构对话**：只做结构映射，不生成任何原文没有的消息内容；
2. **不修改正文**：`text` / `thinking` / `input` / `output` 原样保留；
3. **`seq` 连续无缺口**：在最终顺序确定后统一编号，从 0 起严格递增
   （官方 `validate-log.py` 会检测断档或篡改）。

被跳过的都是非对话记录（`token_count`、`reasoning`、`item_completed`、
`thread_settings_applied`、`task_started/complete` 等内部记账事件），
已在转换脚本中显式列出。

多人协作时，每位成员使用自己的 GitHub login 目录，并在合并前同步最新日志。

## 提交材料包

提交方式：把下表材料打包为 `<队伍名称>-<作品名称>-<仓库名称>.zip`
（源码与 AI Coding 日志在专属仓内，**无需**放进压缩包）。

| 序号 | 材料 | 状态 | 位置 |
| --- | --- | --- | --- |
| 1 | 技术报告（.pdf / .docx） | ✅ 已撰写，待导出 | `docs/SUBMISSION_TECHNICAL_REPORT.md` |
| 2 | 演示视频（≤5 分钟） | ⬜ 待录制 | — |
| 3 | 作品展示照片 | ⬜ 待拍摄 | 建议前/后/侧/俯 + 屏幕特写 |
| 4 | 海报 | ⬜ 可选 | 入围决赛 / 线下展示时提交 |
| 5 | 答辩 PPT | ⬜ 可选 | 入围决赛时提交 |

命名示例：`Dogking-DogkingA733-contest2026_274_Dogking.zip`

## 推送前机械检查

```bash
git status --short
git diff --check
bash -n tools/*.sh
git ls-files | grep -E '\.(img|gguf|a7pm|nb|key|pem)$' && exit 1 || true
git grep -nEi 'api[_-]?key[[:space:]]*[:=]|BEGIN (RSA|OPENSSH|PRIVATE)'
git log --oneline --decorate -10
```

个人远端推送：

```bash
git push share a733-cubie-a7z-share
```

该命令只用于协作备份，不代表已向组委会提交作品。

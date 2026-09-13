# 许可证与来源清单

本项目当前版本：`0.1.0-dev`（见 `product/VERSION`）。项目自身的公开许可证在首次发布前由维护者最终选择；在选择前不把仓库标记为已授权的开源发行版。

| 组件/参考 | 用途 | 当前处理 | 代码复制 |
| --- | --- | --- | --- |
| Agent-First OS `product/` | 自写内核、启动器、用户态协议 | 许可证待发布决策 | 无第三方代码复制 |
| pi (`@earendil-works/pi-agent-core`) | Agent loop 与 tool calling 语义参考 | 上游 MIT；接入时固定 commit 并随发行物保留 NOTICE | 当前仅协议/适配边界 |
| OpenAI Codex app-server | JSONL、线程/事件/审批语义参考 | 按上游仓库许可证核对具体版本 | 当前仅语义参考 |
| Linux、xv6、JOS、seL4、MINIX、TianoCore | 硬件与 ABI 对照 | 只读参考，迁移前逐项记录许可证和来源 | 当前无复制 |

在任何代码、二进制或模型依赖进入发行物之前，必须补充固定版本、许可证文本、NOTICE 和改写范围；构建脚本不下载或隐式 vendoring 第三方源码。

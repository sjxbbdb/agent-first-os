# G9 Runtime Gate 审计（2026-09-14）

本次审计确认 `b2bfad1` 的 native Runtime slice 已将 `AgentOsRuntimeEvent`
别名到现有 `IpcMessage`：`opcode` 位于 offset 8，`sequence` 位于 offset 24，
`words[0]` 位于 offset 32，整体大小为 96 字节。BIOS/OVMF fixture 的两个
Ring 3 进程按 `START → HEARTBEAT → ACK → CHECKPOINT` 方向交换消息；发送端
和接收端均检查 opcode 与 sequence。这里的 sequence 同时作为 IPC envelope
sequence 和当前 bounded fixture 的 `words[0]` payload。

该 gate 只覆盖定宽 IPC transport/lifecycle slice。它没有证明通用服务生成、
task identity/digest 扩展字段、capability 发放、Policy Firewall 审批、持久化
checkpoint、真实 pi/remote model、崩溃恢复、离线模式或硬件/生产行为。G9 的
完整验收仍以 `OVERALL_FRAMEWORK_PLAN.md` 的 Runtime 要求为准，不能将本 slice
标记为 G9 全部完成。

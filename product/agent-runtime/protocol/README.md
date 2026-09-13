# Agent-First OS 用户态协议 v1

这是 G0 冻结的跨模块协议草案。JSON 用于 Supervisor、Policy、Registry 和 Agent Runtime 之间的控制面；内核数据面使用 `product/kernel/include/abi.h`、`ipc.h` 和 `capability.h` 的定宽结构。

所有消息必须带 `version`、`type`、`task_id` 和单调 `sequence`。`ActionPlan` 是 `plan.proposed` envelope 的 payload，外层 envelope 由本地 runtime 生成并做顺序检查。模型返回的数据先解析为 `ActionPlan`，再由 Semantic Registry、Policy Firewall 和内核 capability 检查；任意自然语言、shell 字符串或模型输出都不是可执行接口。

## 核心对象

- `TaskEnvelope`：任务 ID、当前窗口范围、明确文件授权、风险预算和上下文摘要。
- `ActionPlan`：有序动作列表；每个动作包含稳定 `tool_id`、版本、typed 参数、预期后置条件、幂等键和副作用等级。
- `PolicyDecision`：L0–L3 风险、允许/拒绝、所需 capability、一次性 token 摘要和确认来源。
- `JournalRecord`：动作前状态摘要、动作结果、可重试性、可回滚性和外部副作用状态。
- `TaskEvent`：planned、executing、awaiting_confirmation、paused、failed、completed、rollback_pending、unknown_external_effect。

## 不变量

1. `task_id`、`action_id`、资源范围和 capability/token 必须绑定；
2. token 只能由 Policy Firewall 签发、由内核原子消费一次；
3. 任务上下文只包含当前窗口和明确授权的文件；
4. 外部副作用不得在重启后自动重放；
5. offline/null model adapter 不影响内核、维护终端和基础服务。

## 适配器接口

`AgentEngine` 暴露 `start`、`step`、`cancel`、`resume`、`checkpoint`；pi agent 作为首个实现，Codex harness 只通过独立 adapter 提供执行循环、事件和恢复语义。对外始终使用本协议，不把任一框架的内部对象暴露给内核或服务。

JSON Schema 无法表达 canonical digest、action_id 唯一性、token 原子消费和 capability generation；这些约束由本地 verifier/conformance test 强制执行。模型声明的 risk 只是提示，Policy 必须重新计算，幂等键由本地 canonical action 派生。

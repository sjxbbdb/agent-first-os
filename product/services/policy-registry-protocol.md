# Policy Firewall 与 Semantic Registry 协议骨架

`policy-firewall.schema.json` 和 `semantic-tool.schema.json` 冻结 G7/G8 的用户态控制面字段。Registry 只登记稳定 `tool_id + tool_version` 的工具描述和验证器；`version` 是协议 schema 版本，`tool_version` 是可 pin 的工具实现版本。Policy 根据任务、资源摘要、capability generation、风险和有效期重新计算决定。模型提供的 risk 只能作为输入提示，不能直接授权。

Policy 签发的确认 token 是绑定 `task_id + action_id + resource_digest + capability_digest + expiry` 的 opaque 值，由内核或受保护 Policy 服务一次性消费。伪造、重放、过期或跨任务使用必须拒绝；capability 还必须处于有效 generation 且带有动作所需的 `invoke` right。当前阶段只有 host-side schema/运行时替身，真实 kernel syscall、token 原子消费和持久 journal 在 G3/G7 gate 实现。

## Native Ring 3 slice

`product/tests/g8-native-semantic-test.sh` provides the first native service
slice. A Ring 3 Agent sends bounded IPC queries (`0x80` registry lookup,
`0x81` task-window capture, `0x82` action binding, `0x83` Trusted Input) to a
Ring 3 Policy/Registry fixture. An unknown tool receives `0x9f` deny; accepted
queries return versioned tool metadata (`0x90`), context (`0x91`), action
(`0x92`) and Trusted Input (`0x93`) replies. The Agent verifies these replies
before committing.

The fixture proves process placement, IPC ordering, deny-by-default handling,
and task/action binding at the native boundary. Its identifiers are bounded
test values, not cryptographic digests; real device input, persistent Registry
storage, and complete native rollback remain open work.

# G7–G10 host policy / registry / agent loop evidence

日期：2026-09-13

## 运行命令

```text
bash /mnt/d/Agent\ OS/product/tests/g7-g10-host-loop-test.sh
```

输出：

```text
G7-G10 host policy/registry/agent loop: PASS
evidence: events=10 journal_records=14 recovered=1
boundary: host-only reference; no native kernel token, initrd service, or production model claim
```

## 覆盖范围

- `SemanticRegistry` 对版本化工具、provider、参数类型、所需 capability、风险和撤销状态做确定性解析；未授权注册被拒绝。
- `ContextCollector` 只提交当前任务窗口与明确授权文件，未授权文件不会进入上下文。
- `PolicyFirewall` 从 Registry 重新计算风险，不接受模型下调的风险；检查任务风险预算、明确文件范围和 generation-tagged capability（必须带 `invoke` right）。
- L2 动作签发 task/action/resource/capability 绑定的 opaque 一次性 token；错任务、过期、撤销和重放均 fail-closed。
- `VerifiedExecutor` 在动作前写 prepare journal，在后置条件失败时写 `rollback_pending`，成功时写 commit；恢复扫描能识别未完成事务。
- Agent Runtime 通过现有 `AgentEngine` 运行结构化 ActionPlan；malformed plan、未知工具、越权资源和撤销 capability 都不会调用执行器。
- 故障矩阵还覆盖了带 prompt-injection 文本但越过授权文件窗口的动作、断网时的显式
  `OfflineNullAdapter` fallback、Pi loop checkpoint/restart recovery，以及执行器异常
  导致的 `rollback_pending` journal 状态。

## 未验证边界

这是 Node.js host-side reference integration test。它没有证明原生 Ring 3 Policy/Registry、内核 token 原子消费、initrd 启动、真实文件/网络服务、QEMU/OVMF 端到端闭环、pi/Codex 远程模型适配或生产级崩溃持久恢复。该测试不授予模型权限，也不把任何框架代码链接进内核。

# Ring3 Agent Runtime 参考实现

这里是用户态参考实现，用来冻结协议、状态机和适配器边界。它不会被链接进内核，也不授予模型任何特权；真实 OS 中，`executor` 只能通过 Supervisor、Policy Firewall 和内核 capability/IPC 接口执行动作。

实现使用 Node.js 内置模块，便于在没有 TypeScript 工具链时运行。`runtime.js` 提供：

- `AgentEngine`：`start`、`step`、`resume`、`cancel`、`checkpoint` 生命周期；`types.d.ts` 记录供适配器使用的协议类型；
- `PiAgentLoop`：更接近 pi session/event-stream 语义的 Ring 3 loop，提供结构化 model response、一次性 approval、事件背压、heartbeat 监视以及跨实例 checkpoint/recovery；它是无依赖参考实现，不代表已安装或运行真实 pi；
- `DurableCheckpointStore`：host-side JSONL checkpoint/recovery 参考，追加后 `fsync`，重启恢复和损坏尾 fail-closed；它不代表原生文件系统的崩溃一致性；
- `MockModelAdapter`：确定性模型替身，后续可替换为 pi agent adapter 或 Codex harness adapter；
- `adapters.js`：`OfflineNullAdapter`、远程请求边界、pi-primary adapter 和 Codex harness/app-server-shaped adapter；它们只接受/返回结构化 ActionPlan，不把框架代码放入内核；
- `HttpRemoteModelAdapter`：用户态 JSON-over-HTTP 传输，支持超时、错误回退和 ActionPlan 绑定校验；具体模型 provider、认证和生产可用性仍由部署层负责；
- `validateActionPlan`：JSON Schema 字段检查和 action ID 唯一性检查；
- `normalizeActionPlan` / `deriveIdempotencyKey`：本地生成 canonical 幂等键，模型提供的值不受信任；
- JSONL `TaskEvent` 输出，事件带有单调的 `sequence`，便于日志重放和故障定位。

在仓库根目录运行一致性测试：

```powershell
node product/agent-runtime/reference/conformance.test.js
```

预期输出：`agent-runtime conformance: PASS`。

额外的 pi-shaped loop 验证：

```powershell
node product/tests/g9-pi-loop.test.js
```

该测试覆盖 malformed response、approval/recovery、model turn 去重、事件
ack/backpressure 和 heartbeat timeout；它明确停留在 Ring 3 host reference 边界。

G7–G10 的 host-side control-plane 纵切由
`product/services/policy_registry_runtime.js` 和
`product/tests/g7-g10-host-loop-test.sh` 覆盖。它把 Registry、任务窗口、Policy
risk/capability/token 校验、后置条件和 journal recovery 接到同一个
`AgentEngine`，并显式标记为 host-only reference；这条测试不能替代原生 Ring 3
服务、内核 token 原子消费或 QEMU/OVMF 端到端证据。

## 状态机

`created → running → awaiting_confirmation → running → completed` 是成功路径；策略拒绝、非法计划、模型或执行器异常进入 `failed`，用户取消进入 `cancelled`。引擎要求显式注入 `policy` 与 `executor`，没有默认放行或默认成功执行。测试用 `FixturePolicy` 只接受本地 `toolRisks` 中已登记的工具，并按本地风险声明决定是否确认，忽略模型声明的风险。确认 token 由 policy 的 `issueConfirmation` 签发、由 `consumeConfirmation` 校验并消费；引擎只转交 opaque token。L2/L3 默认需要一次性确认 token。真实实现还必须把这两个接口接到 Policy Firewall 与内核原子消费机制。`FixturePolicy` 不是生产 Policy Firewall，不能替代资源授权、capability 或内核校验。

该参考实现刻意没有模拟远端模型、网络、文件系统回滚或硬件驱动。`checkpoint` 目前返回进程内状态快照，不能据此宣称支持崩溃后的持久恢复。它的验收目标是让 G0/G9 的跨模块对象、事件顺序和适配器入口先有可运行的契约测试。

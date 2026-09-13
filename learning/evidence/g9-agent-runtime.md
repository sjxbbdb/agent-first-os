# G9 Agent Runtime 参考实现证据

验证命令：

```text
node product/agent-runtime/reference/conformance.test.js
```

预期输出：`agent-runtime conformance: PASS`。

测试覆盖 ActionPlan malformed 输入、模型风险伪装、Registry 风险重算、Policy 一次性确认 token、事件 sequence、取消和 checkpoint。实现运行在主机用户态，模型凭据、远端网络、native syscall/IPC executor 和 Supervisor heartbeat 尚未接入；pi 与 Codex 只保留适配边界。

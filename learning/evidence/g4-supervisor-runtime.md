# G4 Supervisor 参考运行时证据

`product/services/supervisor_runtime.py` 提供一个只使用 Python 标准库的、可重复的主机侧 Supervisor 状态机。它读取并约束现有 `supervisor-manifest.schema.json` 与 `service-event.schema.json` 的字段，执行依赖 DAG 拓扑排序和循环拒绝，维护 `STARTING`、`READY`、`HEARTBEAT`、`FAULT`、`RESTARTING`、`EXIT`、`FROZEN` 状态，并把事件追加到 JSONL。

`product/tests/supervisor-runtime-test.sh` 注入固定时钟和 nonce 序列，覆盖循环依赖拒绝、拓扑顺序、旧 generation/nonce heartbeat 拒绝、超时 fault、重启次数与 backoff、task group 冻结以及 per-service sequence 单调性。运行命令：

```text
product/tests/supervisor-runtime-test.sh
```

这项证据只证明用户态协议的参考实现和主机可重复测试；它不证明 Ring 3 原生进程、真实 capability 撤销、initrd 启动或 QEMU 端到端行为。

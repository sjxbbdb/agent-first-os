# Supervisor 用户态协议 v1

Supervisor 是第一个用户态服务。它读取只读 initrd 中的服务 manifest，按依赖关系启动服务，并维护每个服务的生命周期、heartbeat 和任务组。本文只定义用户态控制面数据格式；进程、capability 和 IPC 的具体实现由内核 ABI 另行定义。

## Manifest

`supervisor-manifest.schema.json` 是唯一的结构来源。每个 manifest 必须有稳定的 `service_id`、绝对 `entrypoint`、依赖列表、heartbeat 参数和重启策略。`capabilities` 只描述 Supervisor 请求的最小 bootstrap 能力；Supervisor 不能凭 manifest 创造 policy root、kernel admin 或其他新能力。

启动前，Supervisor 做三项确定性检查：所有依赖存在且形成无环 DAG；服务 ID 和能力资源没有重复冲突；`timeout_ms >= interval_ms` 且所有数值在 schema 范围内。检查失败时服务不会启动，并产生 `FAULT` 事件。

## 生命周期与事件

状态序列为：

```text
STARTING → READY → HEARTBEAT*
STARTING → FAULT → RESTARTING → STARTING
READY → FROZEN → READY | STOPPING
READY → EXIT | STOPPING → EXIT
```

服务事件使用 `service-event.schema.json`。`sequence` 在每个服务内单调递增，`generation` 在每次重新启动时递增；旧 generation 的 heartbeat 永远不能令服务恢复为 READY。`HEARTBEAT` 必须携带 nonce，Supervisor 按 manifest 的 interval、timeout 和 grace 判定失联。失联先进入 `FAULT`，再按 restart 策略回收任务组和 capability。

事件是追加式证据。Supervisor 重启后可以从日志重建最后一个状态；它不得根据最后一个 `HEARTBEAT` 自动重放服务的外部副作用。

## 任务组与故障处理

每个服务属于一个 task group。FAULT、EXIT 或紧急暂停会按顺序冻结 task group、取消未完成 IPC、撤销该 generation 的 bootstrap capability、记录事件，再执行回收或重启。重启次数和 backoff 由 manifest 限制；超过上限时保持 FAULT 并等待维护入口。

## 当前原生纵切

`product/tests/g4-native-supervisor-test.sh` 在 BIOS/QEMU 中运行一个真实
Ring 3 Supervisor 和一个 Ring 3 service fixture。Supervisor 通过
`SYS_YIELD` 让 service 发送 IPC heartbeat，收到 service exit 后使用
父子关系受限的 `SYS_RESTART` 恢复同一个初始用户帧，再通过 `SYS_WAIT`
回收第二次退出。另有 BIOS/UEFI initrd version 2 的 manifest/service table
装载证据。通用 service spawn、原生 task-group capability 撤销与 freeze
仍属于后续实现，不应把这些 fixture 当作完整 G4。

`supervisor-g4-group-test.sh` 覆盖 host reference 的依赖 READY 门、显式 group
pause、generation capability 回收和旧句柄拒绝。显式 group pause 阻止新启动；
服务故障只冻结同组 peers，并允许故障服务按 backoff 重启。该行为尚未接到内核。

## 主机验证器

`validate_supervisor_protocol.py` 只使用 Python 标准库，验证 schema 的关键约束及跨字段不变量，并以 canonical JSON 输出 digest，便于教学和 CI 在没有 JSON Schema 依赖时复现。它不声称验证内核行为。

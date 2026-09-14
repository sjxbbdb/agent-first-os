# G7 native emergency pause increment

日期：2026-09-14

本增量把 `SYS_POLICY_PAUSE` / `SYS_POLICY_RESUME` 从单纯的 token gate 扩展为内核强制的任务生命周期边界。策略 authority 仍由 Ring 3 Policy 服务持有；Ring 0 只验证 authority capability，并执行不可绕过的状态转换。

暂停时，内核遍历其他进程：READY 任务被标记为 `FROZEN`；BLOCKED IPC 任务先通过 `agent_os_process_cancel_ipc` 清除 waiter 和缓冲区，再标记为 `FROZEN`。本次暂停设置 `emergency_frozen` 标记，恢复时只将这些任务重新置为 READY，避免误解冻由其他策略冻结的任务。策略 token 的暂停拒绝语义保持不变。

## 验证

```text
wsl bash product/tests/g7-kernel-pause-test.sh
PASS: Ring 3 policy authority pauses/resumes the kernel gate and expiry fails closed
```

串口日志包含：

```text
SYSCALL policy emergency pause OK
SYSCALL policy emergency pause lifecycle OK
SYSCALL policy token consume paused
SYSCALL policy emergency resume OK
SYSCALL policy emergency resume lifecycle OK
SCHEDULER idle - all tasks exited
```

这证明的是单核 BIOS/QEMU 中的任务状态转换与 IPC waiter 取消入口。它没有证明持久化 journal、输入设备停止、真实磁盘事务、SMP 全局暂停或真实硬件行为；这些仍属于 G7/G10 后续增量。

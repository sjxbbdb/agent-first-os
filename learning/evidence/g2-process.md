# G2 进程生命周期模块（主机契约证据）

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g2-process-test.sh'
```

预期输出：`G2 process lifecycle OK`。

当前证据覆盖固定大小进程表、generation 句柄、父子 wait/reap、round-robin 选择、yield/exit/kill 和 stale handle 拒绝。`product/tests/g2-kernel-test.sh` 还证明 BIOS kernel 中两个 Ring 3 fixture 可通过 syscall frame replacement 完成 task 1→task 2→idle；`g2-fault-recovery-test.sh` 证明用户 #PF 可只回收 faulting task 并恢复 task 2。当前仍是 cooperative 单核切片，不等同于动态服务装载、timer 抢占或通用 fault policy。

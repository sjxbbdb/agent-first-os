# G2 内核单核调度与 CR3 证据

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g2-kernel-test.sh'
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g2-wait-kill-test.sh'
```

QEMU 串口已观察到两个 Ring 3 fixture 的 distinct CR3 roots、启动和切换时的 `G2 CR3 switch OK`、`SYS_EXIT` frame replacement、task 1→task 2→idle，以及子进程 `SYS_KILL`→zombie→`SYS_WAIT`→reap。当前是单核 cooperative 调度；通用 ELF 装载、timer 抢占、阻塞唤醒和异常后的 task 回收未完成。

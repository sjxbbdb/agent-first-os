# M7：Ring 3 静态程序与 syscall ABI（切片）

已实现 legacy `.user_text` 兼容段（链接地址 `0x180000`）、DPL3 代码/数据描述符、64 位 TSS/RSP0、DPL3 `int 0x80` IDT 门、`SYS_WRITE=1` 与 `SYS_EXIT=0` ABI，以及内核侧用户指针范围检查。当前 Ring 3 首个 fixture 另由独立 `user.elf` 的受校验 `PT_LOAD` 装载到 `0x400000`，再通过独立 CR3 进入。

验证：

```text
bash product/tests/stage7-test.sh
PASS: Ring 3 enters through TSS/RSP0, exercises int 0x80 validation, and exits
```

运行时串口关键证据：

```text
RING3 launch
USER RING3 OK
SYSCALL write OK
SYSCALL write EFAULT
SYSCALL denied - unknown number
SYSCALL exit OK - user reclaimed
```

默认启动仍输出 `RING3 ABI READY - launch gated pending TSS`，避免普通维护启动自动跳入演示用户程序；`AGENT_OS_TEST_RING3` 会显式安装 TSS/RSP0 并放行测试。当前 QEMU 证据已验证用户输出、`EFAULT`、未知 syscall 拒绝和退出回收。真实硬件、分页隔离和完整进程回收仍未完成。

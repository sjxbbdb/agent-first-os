# G2 legacy IRQ routing evidence

SeaBIOS leaves the 8259 PIC active. Before the kernel owned the PIC, IRQ0
could arrive on legacy vector 8 while Ring 3 had interrupts enabled; the
exception-only stub interpreted the hardware frame as a double-fault frame.
The kernel now has a test-gated timer path: it remaps IRQ0 to vector 32,
programs PIT channel 0, acknowledges the PIC, saves the interrupted Ring 3
frame, and replaces it with the next READY task. The normal path still masks
both controllers. The dedicated preemption test keeps the first embedded user
ELF in a busy loop with no syscall or `SYS_YIELD`; QEMU records a timer-driven
switch to task 2 and then resumes task 1.

验证命令：

```text
wsl.exe bash product/tests/g2-irq-test.sh
```

定向红证据：加入 timer marker 断言但仍使用旧构建宏时，Ring 3 仍能退出，
但日志没有 `TIMER PREEMPT switched task`，测试失败。绿证据：启用
`AGENT_OS_TEST_TIMER_PREEMPT` 后同一脚本通过，日志包含：

```text
TIMER PREEMPT switched task
USER RING3 TASK2 OK
USER RING3 OK
SCHEDULER idle - all tasks exited
```

边界：这是单核、PIC/PIT、固定频率、测试宏门控的抢占切片；没有 APIC、
timer tick accounting、时间片公平性、阻塞唤醒、SMP/TLB shootdown 或真实
硬件验证。生产默认仍保持 IRQ mask fail-closed。

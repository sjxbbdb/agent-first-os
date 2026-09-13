# G3 多等待者阻塞 IPC 证据

命令：

```text
bash product/tests/g3-kernel-multiwait-ipc-test.sh
```

结果：通过 BIOS/QEMU 单核真实 Ring 3 夹具。三个独立 CR3 进程按刻意错开的到达顺序运行：第二个接收者先阻塞，第一个接收者随后阻塞，发送者再发送两条消息。内核按阻塞序列号选择最早等待者，而不是按进程表槽位选择。

串口关键证据：

```text
G2 scheduler READY tasks=3
SYSCALL ipc recv blocked
SYSCALL ipc recv blocked
SYSCALL ipc send wake OK
IPC MULTI RECEIVER 2 OK
SYSCALL ipc send wake OK
IPC MULTI SENDER OK
IPC MULTI RECEIVER 1 OK
SCHEDULER idle - all tasks exited
```

这证明了单 endpoint 上两个阻塞接收者分别获得消息、发送者继续运行且所有任务最终退出。队列仍是有界内核表扫描，尚未实现动态 endpoint、超时/取消 syscall、reply 对象或通用对象回收。

# G3 IPC 取消证据

命令：

```text
bash product/tests/g3-kernel-ipc-cancel-test.sh
```

BIOS/QEMU 单核真实 Ring 3 证据：Supervisor 语义由父进程表示。子进程先在 `SYS_IPC_RECV_WAIT` 阻塞，父进程调用 `SYS_IPC_CANCEL`；内核校验 parent 关系，将保存帧恢复为 `-125/ECANCELED`，子进程继续运行并退出，父进程随后 `wait/reap`。

关键串口 marker：

```text
SYSCALL ipc recv blocked
SYSCALL ipc cancel OK
IPC CANCEL CHILD OK
IPC CANCEL PARENT REAP OK
SCHEDULER idle - all tasks exited
```

该证据覆盖单个父子任务的 IPC 取消；超时参数、动态 endpoint、reply 对象和 task-group 批量取消仍未实现。

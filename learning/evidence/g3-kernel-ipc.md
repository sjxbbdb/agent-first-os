# G3 内核 IPC syscall 证据

`g3-kernel-ipc-test.sh` 在 BIOS/QEMU 中编译 Ring 3 fixture，Ring 0 初始化固定 loopback endpoint 和 generation-tagged capability。用户态通过 `SYS_IPC_CALL` 提交 endpoint capability、opcode 和一个 inline word；随后通过 `SYS_IPC_RECV` 请求 copy-out。内核先检查 capability rights 与对象身份，再调用 IPC endpoint 原语，并逐字节复制到经过地址空间检查的用户缓冲区。

QEMU 串口观察到：

```text
G2 CR3 switch OK
SYSCALL ipc send OK
SYSCALL ipc recv OK
```

这证明了一个真实的 Ring 3 → syscall → capability lookup → endpoint enqueue/dequeue → user copy-out 路径。当前仍是固定测试 endpoint，尚未完成阻塞式 receive/reply、共享内存页表映射和用户 ELF 服务进程。

验证命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g3-kernel-ipc-test.sh"
```

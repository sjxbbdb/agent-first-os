# G3 内核共享内存映射证据

`g3-kernel-shm-test.sh` 构建两个 Ring 3 fixture。Ring 0 将同一个页对齐共享页以 `CAP_RIGHT_MAP` capability 放入两个独立 process namespace；两个进程分别调用 `SYS_SHM_MAP` 把它映射到 `0x500000`。第一个进程写入 `HM PING`，第二个进程从自己的地址空间读取同一内容并通过 `SYS_WRITE` 输出。

QEMU 串口观察到两次 `SYSCALL shm map OK` 和两次 `HM PING`，随后两个任务都退出并进入 idle。这个切片证明了真实物理页共享、capability gate 和用户映射；动态 shared-memory object 创建/撤销、范围派生、跨页检查和阻塞生命周期仍由主机模块及后续 G3 工作覆盖。

验证命令：

```text
wsl.exe bash product/tests/g3-kernel-shm-test.sh
```

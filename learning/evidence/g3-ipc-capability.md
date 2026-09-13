# G3 IPC 与 capability 契约证据

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g3-ipc-caps-test.sh'
```

主机测试覆盖 generation-tagged handle、权限降权、需要 transfer/revoke 权限、撤销后旧句柄拒绝、16 项队列满/空/关闭、ABI 版本、消息长度和 capability 数量边界。

当前模块是可 freestanding 编译的 Ring 0 原语参考实现；每个 `AgentOsProcess` 已拥有独立 capability table，`g3-kernel-ipc-test.sh` 另有 QEMU 证据证明固定 loopback endpoint 的 `SYS_IPC_CALL`/`SYS_IPC_RECV` 已接入 capability lookup、enqueue/dequeue 和用户缓冲区 copy-out；`g3-kernel-shm-test.sh` 证明两个独立 CR3 可以经 capability 映射同一共享页。跨表 capability lineage、阻塞线程唤醒和动态共享对象生命周期仍属于后续 G3 集成证据。

新增的 `g3-kernel-capability-test.sh` 在 QEMU Ring 3 中验证了
`SYS_CAP_RESTRICT` 与 `SYS_CAP_REVOKE`：内核返回新的 generation-tagged opaque
handle，并能在同一进程的 capability namespace 中撤销它。`SYS_CAP_TRANSFER` 的
内核适配也已接入目标进程的独立表；`g3-kernel-cap-transfer-test.sh` 进一步证明
task 1 将 receive-only endpoint capability 转入 task 2 后，task 2 能用新句柄
完成 IPC receive。阻塞 IPC 和动态对象仍未宣称完成。

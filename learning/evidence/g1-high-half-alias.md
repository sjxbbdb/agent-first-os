# G1 高半别名迁移准备证据

## 范围

当前内核仍链接在 `0x00100000`，BIOS 与 UEFI 都继续进入同一个 `kernel_entry`，并从低地址恒等映射执行。此次纵切建立 kernel-owned 高半别名，并加入受限 RIP probe：切换到已构造的进程 CR3，在保持低地址栈不变的情况下，把 `high_half_rip_probe` 调用到高半别名，返回固定 marker 后立即恢复 bootstrap CR3。它证明高半 RIP 可执行与回退路径，不宣称完整高半迁移。

## 可运行验证

在 WSL2 中串行执行：

```text
bash product/tests/g1-high-half-test.sh
```

该脚本先编译并运行 host 页表测试，验证低地址和 `0xFFFFFFFF80000000` 高半地址共享物理页、权限相同且高半映射拒绝 `USER`；随后启用 Ring 3 构建 BIOS 镜像并启动 QEMU，要求串口同时出现：

```text
HIGH HALF alias contract READY low-exec preserved
HIGH HALF RIP probe OK stack low preserved
G2 CR3 switch OK
```

定向红证据是在加入 marker 检查后运行旧实现：host 别名测试通过，但 QEMU 日志没有 `HIGH HALF RIP probe OK stack low preserved`，脚本拒绝通过。实现后同一脚本通过；QEMU 日志还包含两个 Ring 3 fixture 的 `G2 CR3 switch OK`、用户输出和 `SCHEDULER idle - all tasks exited`。

## 边界

- 已验证：host 页表别名与权限契约；BIOS/QEMU 中从进程 CR3 高半别名执行固定无状态 probe；probe 后恢复 bootstrap CR3，低地址 Ring 3 启动继续通过。
- 未验证：高半栈切换、完整 relocation、C 函数/全局数据在高半执行、UEFI/OVMF 在高半执行、SMP/TLB shootdown、任意物理页分配器和真实硬件。

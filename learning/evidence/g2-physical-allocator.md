# G2 bounded physical allocator evidence

`product/kernel/src/physmem.c` now retains up to 16 page ranges derived from
all usable E820 entries. It starts above the reserved low bootstrap scratch
area,
subtracts kernel, staging, initrd and E820-table ranges, and advances across
range boundaries. `phys_allocator_init` remains a bounded bootstrap allocator;
it is not a complete PMM or concurrent frame allocator.

定向红证据：旧实现只保留第一个 usable 区间；host fixture 的第二次分配
无法到达第二个 E820 区间，测试断言失败。绿证据：实现后 host fixture 通过
多个 usable 区间、kernel/initrd 保留区和保留后空洞：

```text
G2 physical allocator OK: multi-region E820 and kernel/initrd reservations
```

验证命令：

```text
bash product/tests/g2-physical-allocator-test.sh
bash product/tests/g2-kernel-test.sh
```

保留区间按页向下/向上包围，避免非对齐 kernel/initrd 尾页泄漏；host fixture
专门覆盖了非对齐边界。BIOS 构建和 G2/QEMU 任务生命周期回归通过，证明新 allocator 初始化不会破坏
现有启动和 Ring 3 路径；当前 QEMU 日志仍只展示 `pmm.ready` 与首个页框，尚未
加入完整多区域串口 marker。

边界：最多 16 个 free ranges、最多 8 个保留范围；保留范围来自 BootInfo
kernel/staging/initrd 和 E820 表本身。当前 `vm.c` 的页表池仍是独立的有界
identity-mapped pool，尚未切换为该 PMM；没有验证 UEFI memory map、并发分配、
内存类型合并、碎片策略、回收或真实硬件。

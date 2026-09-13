# G3 shared-memory 元数据证据

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g3-shm-test.sh'
```

主机测试覆盖页对齐、零长度、整数溢出、范围越界、权限提升和 generation 撤销。当前模块只管理共享内存元数据，尚未执行真实页表映射、跨进程 CR3 切换或 DMA/IOMMU 约束。

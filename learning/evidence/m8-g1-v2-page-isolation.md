# G1：BootInfo v2 与 4 KiB 引导页表

日期：2026-09-13

## 已验证

- BIOS Stage2 填写 `BootInfo.version = 2`，`loader_type = AGENT_OS_BOOT_LOADER_BIOS (1)`；公共 `BootInfo` 类型通过 `boot_info.h` 兼容别名统一指向 `boot_info_v2.h`。
- Stage2 不再把 BIOS GDT 地址写入 BootInfo。内核在 Ring 3 启动前构造并加载自己的 GDT/TSS。
- bootstrap 恒等映射使用 4 KiB PT；legacy `.user_text` 保留在 `0x180000`，嵌入式用户 ELF 运行副本映射在 `0x400000`，两者的用户叶 PTE 才设置 U/S，内核页保持 supervisor-only。
- ELF 程序头循环使用独立的 `r14` 索引，避免 `rep movsb/stosb` 改写 `rcx` 后跳过或重读程序头。
- Ring 3 读取内核页得到 `#PF error=5, CR2=0x100000`；写内核页和写用户代码页得到 `error=7`；执行 NX 用户栈得到 `error=0x15`。这些探针通过 `USER_NASMFLAGS_EXTRA` 注入，证明权限来自实际页表。

## 验证命令

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g0-contract-test.sh'
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/stage8-test.sh'
```

结果：`G0 ABI headers OK`、`PASS: BootInfo v2 boots; Ring 3 works; kernel-page read raises user protection fault`。独立的 `g1-permission-test.sh` 还验证了用户写内核页（error 7）、写用户代码页（error 7）和执行 NX 栈（error 0x15）；QEMU 每次以预期 timeout 结束。

权限探针手动运行结果：读取内核页 `vector=0xe/error=0x5/cr2=0x100000`；写内核页 `error=0x7/cr2=0x100000`；写嵌入式 ELF 用户代码页 `error=0x7/cr2=0x400042`；执行用户栈 `error=0x15/cr2=0x1feff8`。

## 边界

这是单核、低端恒等映射的引导隔离基线；完整进程地址空间、高半迁移、回收和调度仍属于后续 G2 工作。当前 page-table walk 只覆盖 bootstrap 低端映射，不能作为通用地址空间或真实硬件支持声明。

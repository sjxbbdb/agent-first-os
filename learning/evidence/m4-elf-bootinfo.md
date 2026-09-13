# M4：ELF64 与 BootInfo 验证

日期：2026-09-13

## 构建

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tools/build-bios.sh"
```

结果：

```text
built /mnt/d/Agent OS/build/bios/stage1.img (14568 bytes: stage1=512, stage2=1248/8192, kernel=5864 bytes/12 sectors, kernel_lba=17; signature 0x55AA)
```

构建产物包含 512 字节 Stage 1、固定 16 扇区 Stage 2 和 ELF64 内核。Stage 2 将内核暂存到 `0x10000`，校验后把 `PT_LOAD` 段复制到 `0x100000..0x200000`，并在 `0x6000` 写入 112 字节 `BootInfo`。

## 正常启动

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tools/run-bios.sh"
```

串口证据：

```text
S1 BIOS OK - stage2 pending
S2 BIOS OK - stage2 loaded
S2 64BIT OK - long mode
KERNEL ELF OK - jumping to kernel
KERNEL C OK
boot_info.magic=0x41474f53424f4f54
boot_info.version=0x0000000000000001
kernel=[0x0000000000100000, 0x000000000018004b]
verified BIOS boot chain, long mode, ELF handoff, and C kernel markers; QEMU timeout was expected
```

## 负向启动

`product/tests/stage4-test.sh` 将 ELF 起始 magic 改为零后运行同一镜像：

```text
S2 64BIT OK - long mode
S2 ERROR - invalid kernel ELF
PASS: invalid ELF is diagnosed in Stage 2 and never reaches the kernel
```

同一轮 `stage1-test.sh`、`stage2-test.sh`、`stage3-test.sh` 继续通过：坏签名、错误 Stage 2 LBA 和不支持 long mode 都在对应层停止。

完整回归命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/stage1-test.sh && bash product/tests/stage2-test.sh && bash product/tests/stage3-test.sh && bash product/tests/stage4-test.sh"
```

## 已知边界

- 当前页表只恒等映射低 2 MiB；高半内核和完整物理内存图属于 M6。
- ELF 校验覆盖 magic/class/data/machine、程序头边界、整数溢出、`p_filesz <= p_memsz`、可装载地址范围和可执行入口；段重叠拒绝的专门测试尚未加入。
- BIOS/SeaBIOS 路径已在 QEMU 验证；UEFI、真实硬件、SMP 和中断仍未验证。

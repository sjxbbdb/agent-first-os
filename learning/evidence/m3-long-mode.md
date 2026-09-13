# M3：x86_64 长模式入口验证

## 范围

Stage 2 在实模式检查 CPUID/long mode，开启 A20，通过 GDT 进入保护模式，清理并建立低 2 MiB identity 页表，开启 PAE、EFER.LME 和分页，最后跳入 64 位代码并通过 COM1 输出标记。中断保持关闭，尚未进入 C 内核。

## 命令与结果

```powershell
pwsh -NoProfile -File product/tools/build-bios.ps1 -Run
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage3-test.sh'
```

正向输出：

```text
S1 BIOS OK - stage2 pending
S2 BIOS OK - stage2 loaded
S2 64BIT OK - long mode
verified Stage 1, Stage 2, and long-mode markers; QEMU timeout was expected
```

负向输出（`-cpu qemu32`）：

```text
S1 BIOS OK - stage2 pending
S2 BIOS OK - stage2 loaded
S2 ERROR - long mode unavailable
PASS: unsupported long mode is diagnosed and halted before mode switch
```

## 通过项

- A20、GDT、保护模式、PAE、EFER.LME、CR0.PG 和长模式入口已在 QEMU 中走通；
- 页表映射覆盖启动代码、栈和 Stage 2 使用的低地址区域；
- long mode 不支持时会显式报错并停机；
- 仍使用 COM1 作为可复现日志通道。

## 未验证项

- 尚未安装 IDT 或开启中断；
- 尚未加载 ELF64 内核；
- 尚未创建或传递 BootInfo；
- 尚未启用高半内核映射；
- 尚未在真实硬件或 UEFI/OVMF 路径验证。

# Product build tools

整体框架的 G0 契约检查：

```text
wsl.exe bash '/mnt/d/Agent OS/product/tests/g0-contract-test.sh'
```

它会编译检查 `abi.h`、`boot_info_v2.h`、`ipc.h`、`capability.h` 的静态断言，并解析 Agent Runtime 的 ActionPlan schema。

当前已实现 gate 的串行回归入口（脚本会复用并注入 `build/bios`，不要并行启动这些测试）：

```text
wsl.exe bash '/mnt/d/Agent OS/product/tests/run-all.sh'
```

G2/G3 的重点垂直测试也可以单独运行，但仍应串行执行：

```text
wsl.exe bash '/mnt/d/Agent OS/product/tests/g2-elf-loader-test.sh'
wsl.exe bash '/mnt/d/Agent OS/product/tests/g2-irq-test.sh'
wsl.exe bash '/mnt/d/Agent OS/product/tests/g3-kernel-ipc-test.sh'
wsl.exe bash '/mnt/d/Agent OS/product/tests/g3-kernel-shm-test.sh'
```

当前工具入口覆盖 BIOS Stage 1 和 Stage 2：

```powershell
pwsh -File product/tools/build-bios.ps1
pwsh -File product/tools/build-bios.ps1 -Run
```

M2 负向验证（错误 Stage 2 LBA）：

```powershell
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage2-test.sh'
```

M3 负向验证（禁用 long mode）：

```powershell
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage3-test.sh'
```

M5 负向验证（内核除零异常）：

```powershell
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage5-test.sh'
```

负向验证（破坏启动签名后不得进入 Stage 1）：

```powershell
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage1-test.sh'
```

PowerShell 负责调度，实际的 NASM 和 QEMU 命令在 WSL2 中运行。构建输出位于被忽略的 `build/bios/`，运行日志按镜像名写入 `build/bios/qemu-*.log`。

Stage 1/2 的 M1–M2 合同：

- Stage 1 严格 512 字节；Stage 2 当前固定占用 16 个扇区，raw 镜像随后放置 ELF64 内核；
- Stage 1 扇区末尾为 `0x55AA` 启动签名；
- 初始化 `DS/ES/SS/SP`，清除方向标志；
- 保存 BIOS 传入的 `DL`；
- 通过 VGA teletype 和 COM1 输出 `S1 BIOS OK - stage2 pending`；
- 使用 EDD `INT 13h/AH=42h` 读取固定 LBA 的 Stage 2，失败时重试、复位并输出诊断；
- Stage 2 在 `0000:8000` 输出 `S2 BIOS OK - stage2 loaded`，进入 long mode 后输出 `S2 64BIT OK - long mode`；不支持 long mode 时输出错误并 halt。

Stage 2 只在启动阶段使用 BIOS 中断，完成 ELF64 装载后通过版本化 `BootInfo` 进入内核；最终内核不依赖 BIOS 中断。

内核构建可通过 `KERNEL_CFLAGS_EXTRA` 注入教学用编译宏；M5 测试使用
`-DAGENT_OS_TEST_DIV0` 触发向量 0，验证 IDT 日志和停机路径。

G5 UEFI Stage0（PE/COFF + OVMF marker）：

```text
wsl.exe bash '/mnt/d/Agent OS/product/tests/g5-uefi-stage0-test.sh'
```

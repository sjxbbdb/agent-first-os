# M2：BIOS Stage 2 与 EDD 读取验证

## 范围

Stage 1 通过 BIOS EDD `INT 13h/AH=42h` 读取固定 LBA 1 的 Stage 2 到 `0000:8000`，成功后跳转；失败时复位磁盘并重试三次。

## 命令与结果

```powershell
pwsh -NoProfile -File product/tools/build-bios.ps1 -Run
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage1-test.sh'
wsl.exe bash '/mnt/d/Agent OS/product/tests/stage2-test.sh'
```

实际结果：

```text
built /mnt/d/Agent OS/build/bios/stage1.img (1024 bytes: stage1=512, stage2=512; signature 0x55AA)
S1 BIOS OK - stage2 pending
S2 BIOS OK - stage2 loaded
verified Stage 1 marker; QEMU timeout was expected
PASS: valid Stage 1 boots; bad signature does not reach Stage 1
S1 BIOS OK - stage2 pending
S1 BIOS disk read failed
S1 BIOS disk read failed
S1 BIOS disk read failed
PASS: invalid Stage 2 LBA is retried, diagnosed, and never jumps to Stage 2
```

## 通过项

- Stage 1 和 Stage 2 各自严格 512 字节；
- 第一扇区签名为 `0x55AA`；
- 正常镜像能完成 `S1 → S2` 交接；
- 错误 LBA 会触发重试、磁盘复位和诊断消息；
- 错误 LBA 不会跳转到 Stage 2；
- Stage 2 仍停留在实模式并进入明确 halt 循环。

## 未验证项

- 尚未进入保护模式或长模式；
- 尚未读取 E820 内存图或 ACPI；
- 尚未加载 ELF64 或创建 BootInfo；
- 尚未验证真实硬件 BIOS 行为。

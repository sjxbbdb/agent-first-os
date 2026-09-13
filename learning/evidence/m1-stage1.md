# M1：BIOS Stage 1 验证

## 范围

本次只验证自写 512 字节启动扇区。Stage 1 现在作为两扇区镜像的第一个扇区，由 M2 的构建入口一起打包。

## 命令

```powershell
pwsh -File product/tools/build-bios.ps1
pwsh -File product/tools/build-bios.ps1 -Run
```

## 通过条件

- 生成的 `build/bios/stage1.bin` 为 512 字节；
- Stage 1 的最后两个字节为 `55 aa`；
- QEMU 使用 SeaBIOS 从 raw 镜像启动；
- 串口日志出现 `S1 BIOS OK - stage2 pending`；
- QEMU 因 Stage 1 的预期 halt 循环在超时后结束。

## 未验证项

- Stage 1 的 EDD 加载由 M2 覆盖；
- 尚未进入保护模式或长模式；
- 尚未加载 ELF64 或创建 BootInfo；
- 尚未验证真实硬件 BIOS 行为。

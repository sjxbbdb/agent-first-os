# g2-irq 根因分析 复核记录（验收审查）

日期：2026-11-02（Asia/Shanghai）　被审对象：`G2-IRQ-ANALYSIS.md`（codex工具人；入库为 round6）
结论：**已查明，限定在 TCG（QEMU 10.0.13）下成立；KVM 未测。**
说明：本文是我自己在盒子上复跑的记录。路径里的 `<EVAL>` 指评估目录（codex工具人的 afos-eval），`<LOGS>` 指本目录的 `logs/`。我没有修改任何内核源码，也没有改评估目录里的文件，复跑产物全放在 /tmp 和本目录。

## 环境
- QEMU 10.0.13（Debian 1:10.0.13+ds-0+deb13u1），无 KVM，TCG。
- OVMF：`<EVAL>/ovmf/root/usr/share/OVMF/OVMF_CODE_4M.fd` 与 `OVMF_VARS_4M.fd`（每次复制一份新的 VARS）。
- 被测镜像都用评估目录里现成的，没有重新构建：`diag/irq-orig.img`、`diag/irq-wait100x.img`、`probe/bios_probe_*.img`、`diag/uefi-orig/esp`、`diag/uefi-wait100x/esp`（ESP 复制到 /tmp 再挂，不改原件）。

## 复跑命令与结果

### 1. BIOS，原始镜像与 10 亿次诊断镜像（`-machine pc`，6 秒，`-d int`）
```
timeout 6 qemu-system-x86_64 -drive format=raw,file=<IMG>,if=ide,snapshot=on -boot c -display none \
  -serial file:<NAME>.ser -monitor none -d int -D <NAME>.tr -machine pc
```
| 镜像 | Ring 3 期间 v=20（cpl=3） | TIMER PREEMPT | SCHEDULER idle |
|---|---|---|---|
| irq-orig.img | 0 | 0 | 1 |
| irq-wait100x.img | 50 | 1 | 1 |
日志：`<LOGS>/orig.{ser,tr}`、`w100.{ser,tr}`。orig.ser 的 sha256 与评估目录 `diag/runs/base-pc.serial.log` 相同。

### 2. BIOS 对照探针（`-machine pc`，8 秒）
```
timeout 8 qemu-system-x86_64 -drive format=raw,file=probe/bios_probe_<LOOPS>.img,if=ide,snapshot=on \
  -boot c -display none -serial file:p<LOOPS>.ser -monitor none -machine pc
```
- LOOPS=10000000：`irq0 during loop=0x00000000`，之后 `total=0x00000014`
- LOOPS=1000000000：`irq0 during loop=0x00000031`（49），之后 `total=0x00000032`
codex工具人 的原日志里 10 亿次是 50（0x32），我复跑是 49，差 1 次，属运行间波动。日志：`<LOGS>/p10000000.ser`、`p1000000000.ser`。

### 3. UEFI（q35 + OVMF，15 秒，`-d int`），用 diag/uefi-orig 与 diag/uefi-wait100x 的 ESP 副本
```
cp OVMF_VARS_4M.fd /tmp/g2rv/<N>.vars; cp -r diag/<N>/esp /tmp/g2rv/esp-<N>
timeout 15 qemu-system-x86_64 -machine q35 -m 128M -nodefaults -device VGA \
  -drive if=pflash,format=raw,readonly=on,file=OVMF_CODE_4M.fd -drive if=pflash,format=raw,file=/tmp/g2rv/<N>.vars \
  -drive format=raw,file=fat:rw:/tmp/g2rv/esp-<N> -serial file:<N>.ser -display none -no-reboot -d int -D <N>.tr
```
参数取自 `product/tests/g5-uefi-kernel-test.sh` 的 QEMU 行（该行把串口接 stdio，我改成了写文件）。
| ESP | cpl3 v=20 | cpl0 v=20 | TIMER PREEMPT | SCHEDULER idle |
|---|---|---|---|---|
| uefi-orig | 0 | 79 | 0 | 1 |
| uefi-wait100x | 50 | 79 | 1 | 1 |
codex工具人 的原日志是 cpl3 为 51、cpl0 为 81，差在运行间波动。日志：`<LOGS>/uefi-orig.{ser,tr}`、`uefi-wait100x.{ser,tr}`；两个 .ser 的 sha256 与评估目录 `diag/uefi-orig/serial.log`、`diag/uefi-wait100x/serial.log` 相同。

## 静态核对
- 镜像差异：`cmp -l diag/irq-orig.img diag/irq-wait100x.img` 只有 12 个字节不同，3 处，每处是立即数 0x989680（10000000）变成 0x3B9ACA00（1000000000），与 `diag/irq-wait-100x.patch` 一致。
- `src/product/kernel/arch/x86_64/user_program.asm:1477` 为 `mov ecx, 10000000`。
- 对照探针与内核的 PIC、PIT、IDT 门配置：对了 `probe/bios_probe.asm`、`probe/uefi_probe.c` 与 `kernel.c:392-402`，ICW 字节与顺序、屏蔽字 0xFE/0xFF、PIT 0x34 加 0xFFFF、门属性 0x8E、EOI 发往 0x20 一致。
- UEFI 的 cpl0 v=20（79 到 81 条）：IP 全在固件地址段（0x5 到 0x7 开头）、CS 为 0x38，没有一条落在内核地址段，所以来自 OVMF，不是内核。
- icount 换算：循环每次 2 条指令，shift=0、1、2 的窗口是 20、40、80 ms，与「shift=2 起翻转」吻合。

## 证据限定与缺陷
1. 「1000 万次约 27 ms」是推算值（探针 10 亿次跨约 50 个 tick），没有直接计时。
2. 「相位也不占优」是推断，没做相位实验。
3. 「宿主越快越容易失败」「作者环境宿主可能更慢」没有跨宿主实验，是推测。
4. KVM 没测，结论只对 TCG 成立。
5. 50 个 tick 只打印 1 次 TIMER PREEMPT，原因没查（我复跑也是 1 次）。
6. 对照探针不走内核的 Ring 3、TSS、中断入口桩，只证明 PIC、PIT、IDT 硬件路径与窗口长度；内核自己的 Ring 3 中断路径由诊断镜像的 50 次送达证明，两者合起来才成立。
7. 报告引用的 `logs/tests/g2-irq-test.sh.log` 是 0 字节，引用路径错误，不影响结论（base-pc 串口最后一行是 SCHEDULER idle，无 TIMER PREEMPT）。`diag/runs/uefi-*-12s.serial.log` 是 BIOS 输出的副本，不能当 UEFI 证据，本复核没有使用。
8. 对 M1 的影响（D1）：内核只处理来自 Ring 3 的向量 32（kernel.c:1357），内核态 IF 恒为 0，idle 是 `cli; hlt`，任务退出后 tick 停止。已写入 v0.7 的 T-M1-1 第 6a 条与 T-M1-3 第 10 条。

## 校验
`SHA256SUMS` 是 `logs/` 下十个文件的哈希，在 `logs/` 目录里用 `sha256sum -c ../SHA256SUMS` 核对。

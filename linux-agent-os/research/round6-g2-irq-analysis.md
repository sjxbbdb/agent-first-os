> 入库说明（方案规格）：本文是 codex工具人 对 g2-irq 失败的根因调查报告，正文基本未改，只在本说明里记录了入库时发现的问题，供审查参考。
> - 状态：验收审查已复核（复核结论经总经理转达，我没有看到他的复核原始记录）：根因已查明，**限定在 TCG（QEMU 10.0.13）下成立**，KVM 没测（没有权限），KVM 下是否仍成立没有证据；他自己复跑了关键对照，结果一致。
> - 正文里的 `logs/`、`diag/`、`probe/`、`src/` 等相对路径指向评估目录，这些文件没有入库；关键文件的 sha256 见 `research/round6-g2-irq-key-sha256.txt`。
> - 正文第 13 行引用的 `logs/tests/g2-irq-test.sh.log` 在入库时核对是 0 字节，没有那行串口输出；同样的现象见 `diag/runs/base-pc.serial.log`（最后一行是 `SCHEDULER idle - all tasks exited`）和 round5 的逐项汇总表（g2-irq 一行 rc=1）。
> - UEFI 路径的串口证据在 `diag/uefi-orig/serial.log` 和 `diag/uefi-wait100x/serial.log`（前者没有 `TIMER PREEMPT`，后者有）。`diag/runs/uefi-orig-12s.serial.log` 与 `diag/runs/uefi-wait100x-12s.serial.log` 两个文件内容和 `base-pc.serial.log` 逐字节相同（都是 BIOS 的输出），不能当 UEFI 证据，也没有列入哈希清单。
> - 入库时按验收审查意见改了正文几处，改动处用「【入库修订】」标出：27 ms 的来历、「宿主越快越容易失败」类说法、对照探针的证明范围、icount 建议。其余正文未改。
> - 诊断补丁（10 亿次循环）只在副本里用过，不是修复建议，也没跑回归。

# g2-irq 失败根因分析

**结论：已查明。** 原因是“测试时序”：用户态等待窗口太短，而且窗口长度取决于宿主机 CPU 速度。中断配置本身没问题（这一点已用对照组验证），也不是 QEMU 或盒子环境缺东西。

- `AGENT_OS_TEST_IRQ_WAIT` 在 Ring 3 里只跑 1000 万次 `dec/jnz`（user_program.asm:1477）。在这台盒子的 TCG 下约 27 ms。【入库修订】这个数不是直接计时的，是用探针跑 10 亿次循环跨过 50 个 tick（约 2.75 s）推算出来的，见实验 E。
- 定时器周期是 65535/1193182 ≈ 54.9 ms（kernel.c:399-401 固定写死）。
- 所以两个任务都在第一次 IRQ0 到来之前就跑完了。随后内核在 kernel.c:210 执行 `cli; hlt` 进入 idle，之后再也不会响应中断。
- 结果就是 Ring 3 期间收到的 0x20 向量为 0 次，串口里也就没有 `TIMER PREEMPT switched task`。
- 【入库修订】原文这里写「宿主越快越容易失败，作者环境能通过说明那边宿主更慢」。没有做过跨宿主的实验，这是推测，已降级；报告只在这一台盒子的 TCG 下做了实验。
- 只要把用户态窗口拉长，或用 icount 让虚拟时间变慢，**不改任何中断代码**，0x20 就能在 cpl=3 下送达，并打出 `TIMER PREEMPT switched task`（只在诊断副本上做过，原仓库没改）。

## 失败现象
- 原测试：logs/tests/g2-irq-test.sh.log。串口最后一行是 `SCHEDULER idle - all tasks exited`，`TIMER PREEMPT` 这一行没有出现。
- 复现：diag/runs/base-pc.*。trace 里向量统计只有 `6 v=80`（int 0x80 系统调用）；0x20 是 0 次，0x08 是 0 次，0x00–0x1f 也没有任何异常。

## 代码路径（以 src 拷贝为准）
- product/tests/g2-irq-test.sh：
  - 宏：`-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_TIMER_PREEMPT`，加 `USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_IRQ_WAIT`。
  - QEMU 参数：**只跑 BIOS**（`-machine pc`、stage1.img、5s 超时、`-d int`），没有 -cpu、-accel、-icount。
  - 断言：要有 `SCHEDULER idle`，不能有 `EXCEPTION`，要有 `TIMER PREEMPT switched task`。
- kernel.c:386 `pic_mask_all`；kernel.c:1398-1401 依次执行 mask_all、interrupts_init、pic_timer_init。
- kernel.c:392-402 `pic_timer_init`：ICW 0x11 / 0x20,0x28 / 4,2 / 1,1；mask 0xFE/0xFF（只开 IRQ0）；PIT 写 0x34，分频值 0xFFFF。
- kernel.c:457 异常门属性 0x8E；kernel.c:470 int 0x80 门属性 0xEE；interrupts.asm:67 `isr_stub_timer` 压入向量 32；interrupts.asm:157 是 IDT 表项。
- 内核里没有显式 `sti`。IF 只在进入 Ring 3 时打开：process.c:123 和 process.c:447 把 `rflags=0x202`，经 iretq 生效。内核态 IF 一直是 0。
- kernel.c:1357-1361：向量 32 且来自 cpl3 时，发 EOI（`outb(0x20,0x20)`），再调用 `preempt_user_timer`。
- kernel.c:268-280：只有成功切换到另一个任务时，才打印 `TIMER PREEMPT switched task`。
- kernel.c:210：idle 路径是 `cli; hlt`。
- user_program.asm:1472-1481：IRQ_WAIT 循环 `mov ecx, 10000000`。源码注释里写的是“comfortably above one legacy PIT period”，但这个前提在快的宿主上不成立。

## 对照实验（BIOS 路径，除标注外都用原始镜像 diag/irq-orig.img）
脚本：diag/run-variant.sh。原始结果在 diag/variants-1.txt，各次日志在 diag/runs/<名称>.{serial,trace}.log。“cpl3 v20”指 Ring 3 期间收到 0x20 的次数。
| 名称 | 变量 | cpl3 v20 | TIMER PREEMPT | 说明 |
|---|---|---|---|---|
| base-pc | -machine pc，5s | 0 | 无 | 复现失败 |
| pc-20s | 运行 20s | 0 | 无 | 时间加长没用：内核早已 idle，并且执行了 cli;hlt |
| pc-i440fx-8.2 | 旧机型版本 | 0 | 无 | 排除机型版本因素 |
| cpu-qemu64 / cpu-max | -cpu | 0 | 无 | 排除 CPU 型号因素 |
| q35 | -machine q35 | 0 | 无 | if=ide 在 q35 上，内核根本没启动（idle=0），trace 里的 v=08/v=70 来自 SeaBIOS 阶段。这一项不能当对照 |
| icount0 / icount1 | shift=0/1（每条指令 1/2 ns） | 0 | 无 | 窗口约 20/40 ms，仍短于 54.9 ms |
| icount2 | shift=2（每条指令 4 ns） | 1 | **有** | 窗口约 80 ms，超过一个周期 |
| icount-auto | shift=auto | 5 | **有** | |
| wait100x-pc-5s/30s | **诊断补丁**：循环改为 10 亿次，其他完全不变 | 50 | **有** | 中断代码一行没改就能送达 |

icount 从 shift=1 不通过、到 shift=2 通过，这个分界正好落在 54.9 ms 的周期上，跟“窗口长度”这个解释吻合。

## UEFI 路径（单独做的，没有沿用 BIOS 的结论）
g2-irq-test.sh 本身只走 BIOS。为了单独验证，我把同一组宏编出的 kernel.elf 放进 uefi_loader（build-uefi.sh）的 ESP，用 OVMF、q35、15s 启动。产物在 diag/uefi-orig/ 和 diag/uefi-wait100x/，结果在 diag/variants-uefi.txt。
| 构建 | cpl3 v20 | cpl0 v20 | TIMER PREEMPT |
|---|---|---|---|
| 原始（1000 万次） | 0 | 79 | 无 |
| 诊断补丁（10 亿次） | 51 | 81 | **有** |

cpl0 下的 v20 来自 OVMF 自己在 ExitBootServices 之前的定时器，跟内核无关。UEFI 路径的结论和 BIOS 一致。

## 对照组（probe/）
- probe/bios_probe.asm（引导扇区，32 位保护模式）和 probe/uefi_probe.c（先 ExitBootServices，再进 64 位）做的是同一件事：
  - PIC/PIT 写入的字节**与 kernel.c:394-401 完全相同**；
  - 0x20 门属性用 0x8E；
  - 执行 sti，然后跑 LOOPS 次 `dec/jnz`，统计循环期间收到的 IRQ0 次数；
  - 循环结束后再用 hlt 等 IRQ0。
- QEMU 参数：BIOS 用 `-machine pc`（和 g2-irq 相同）；UEFI 用和 g5-uefi-kernel 相同的 OVMF/q35 参数。
- 结果（diag/probe-bios.txt、diag/probe-uefi.txt，串口原文）：
  - BIOS，LOOPS=1000 万：`PROBE loop done, irq0 during loop=0x00000000`，之后 `PROBE IRQ0 vector 0x20 received total=0x00000014`
  - BIOS，LOOPS=10 亿：`irq0 during loop=0x00000032`（50 次）
  - UEFI，LOOPS=1000 万：`irq0 during loop=0x0000000000000000`，之后 total=0x14
  - UEFI，LOOPS=10 亿：`irq0 during loop=0x0000000000000032`
- 逐项对照内核：

| 环节 | 探针 | 内核 | 判断 |
|---|---|---|---|
| PIT 配置 | 0x34，0xFFFF | 0x34，0xFFFF | 相同 |
| PIC 重映射 | 0x20/0x28 | 0x20/0x28 | 相同 |
| IRQ0 mask | 0xFE/0xFF | 0xFE/0xFF | 相同 |
| IDT 门 | 0x8E | 0x8E | 相同 |
| EOI | 发给 0x20 | 发给 0x20 | 相同 |
| IF 打开的时机 | sti 后一直开着 | 只在 Ring 3 窗口期间开（rflags 0x202），idle 时执行 cli | **唯一差异** |

- 【入库修订】对照探针只证明了 PIC、PIT、IDT 这条硬件路径本身能收到 IRQ0，以及窗口长度决定能收到几次；内核自己的 Ring 3 中断路径，是靠诊断镜像（循环改 10 亿次）在 BIOS 收到 50 次、UEFI 收到 51 次来说明的。两者合起来，结论才成立，探针单独不能证明内核路径没问题。
- 用实验验证这处差异：同样 1000 万次的窗口，探针也是 0 次；探针在循环后多等的那段时间里能收到 IRQ0，而内核在那段时间已经关了中断。把内核窗口拉长（wait100x）后，Ring 3 期间就收到了 50 次 0x20。

### 实验 E：墙钟时间换算
- PIT 周期 = 65535 / 1193182 Hz ≈ 54.9 ms。
- 探针 10 亿次循环跨过 50 次 tick，约 2.75 s，所以 1000 万次 ≈ 27 ms。
- 27 ms < 54.9 ms，而且 PIT 是在 pic_timer_init 时开始计数的，相位也不占优，所以第一次 tick 来之前窗口就结束了。

## 诊断补丁
- diag/irq-wait-100x.patch：只把 user_program.asm:1477 的 `10000000` 改成 `1000000000`。
- 补丁只在构建 diag 镜像时临时打在 src-diag 上，构建完已经还原；src 拷贝始终是原样。
- **这是诊断补丁，不是正式修复建议。** 对其他测试的影响没验证（IRQ_WAIT 只有 g2-irq 用到，但这一点没跑回归确认）。

## 对 M1 抢占计数验收的影响
- 现有断言只要求至少出现 1 次 `TIMER PREEMPT`，而窗口长度是固定循环次数，实际时长取决于宿主执行速度，所以用它做抢占计数验收不可靠。【入库修订】原文在此还有「快的宿主稳定失败、慢的宿主偶尔通过」，没有跨宿主实验，已删。
- 一点额外观察：wait100x 期间收到了 50 次 0x20，但 `TIMER PREEMPT` 只打印 1 次。原因是只有存在另一个就绪任务时才会切换并打印。以后如果按次数验收，要另外定义口径。这一点只看到了现象，没深查。

## 下一步建议（未实施）
- 把等待窗口和 tick 绑定，而不是绑定循环次数。比如在用户态等内核报告至少 N 次 tick；或者把 PIT 分频调小，让频率更高。
- 【入库修订】原文还建议在测试里固定 `-icount shift=auto` 或 `shift=2` 及以上。总经理按验收审查意见定：这不作为 M1 的办法，M1 用以 tick 数为条件的等待。
- 这两种办法都只是建议，都没有实测回归。

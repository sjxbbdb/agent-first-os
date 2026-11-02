> 入库说明：验收审查 t-m1-1 的数据记录原文，只做路径、构建用户名、账号的替换（见 `research/round7-stage-reviews.md` 第二节）。日志文件没有入库，哈希见 `research/round7-review-logs-sha256.txt`。

# T-M1-1 正式验收 + stage7 期望变更（t-m1-1-r4 = 8fb9198，基于 cc80bb3）
时间 2026-11-02 18:25 起（UTC+8）。克隆 <临时目录>/r4/t1a、t1b；原始输出 logs/。规格依据：<规格仓库> origin/main（fetch 后为 494db93，v0.8）。本地另有未推送的 599e2da（v0.9），它对 T-M1-1 行没有改动（diff 已核），所以两版的 T-M1-1 一样。

## B1 规格逐条摘录与对照
任务清单 T-M1-1 原文（origin/main）逐条摘录，后附对照：
| # | 规格原文（摘） | 代码/日志证据 | 判定 |
|---|---|---|---|
| 1 | 根因报告：已有并已复核（round6）…限定 TCG 下成立，KVM 未测，文档里不得写成「KVM 下也成立」 | 报告写的是「只用 TCG，没有 KVM」「KVM 没测」，没有写成 KVM 下也成立 | 符合 |
| 2 | 默认镜像里定时器抢占开启，不靠测试宏 | kernel.c：`pic_timer_init()` 去掉了 `#if AGENT_OS_TEST_TIMER_PREEMPT`，无条件调用（kernel_main）；`AFOS_M1_PREEMPT` 默认 1；默认镜像串口有 `AFOS-M1 config preempt=on`、`preempt=16` | 符合 |
| 3 | 内核暴露计数：各任务运行次数、定时器抢占次数、主动让出次数，并在串口输出 | `AfosSchedCounts`（tick/tick_cpl3/tick_cpl0/tick_idle/preempt/preempt_noready/yield/spurious/irq7/run[]），由 `sched_counts_print` 打印 `SCHED counts:` 行（irq7 不打印） | 符合（irq7 计数不打印，次要） |
| 4 | 对照组：同一提交、只关抢占的另一冻结构建（单独冻结并附 SHA256SUMS），计数显示抢占 0、只有主动让出；不用运行期开关 | `--variant=nopreempt` 编译期加 `-DAFOS_M1_PREEMPT=0`；冻结目录 afos-m1-t1-8fb9198-nopreempt 有 44 条 SHA256SUMS；串口 preempt=0、yield=8。B2 核过与默认只差开关 | 符合 |
| 5 | 两个任务输出交错且抢占次数>0，不靠肉眼；等待不得依赖固定循环次数，改成「收到至少 N 次 tick 再退出」，N 事先定 | user_program.asm：每轮读 SYS_CLOCK_TICKS，等到差值 ≥M1_WAIT_TICKS=3 才往下走；r14=2^32 次读数只作「时钟停了」的兜底，超出就打印 WAIT TIMEOUT 并退出。m1-t1-check.sh 机器判定 8 行 TASK、交替 ≥2、preempt≥1 | 符合（见 B4：计数确定性的含义） |
| 5a | 先出方案交总经理：「TIMER PREEMPT」打印口径重新定义…方案里写清各计数含义与断言 | 默认镜像不再逐次打印 TIMER PREEMPT（只在 AGENT_OS_TEST_TIMER_PREEMPT 下保留），改打计数行。报告里有阈值表和计数含义；「方案交总经理并获批」我没有找到独立记录（报告提到「计划第 4/7 节」，计划文件不在被审物里） | 无法判断（方案是否获批没有证据） |
| 6 | 同一命令复跑两次结果一致（时间戳除外） | 我跑的默认 5 次、nopreempt 5 次，串口 md5 全同，也与冻结串口相同（B3） | 符合 |
| 6a | idle 与等待期间照常收 tick；idle 用 sti;hlt；向量 32 在 Ring 0 不进异常路径；不出现 EXCEPTION observed；用计数证明 idle 期间 tick 增加 | `scheduler_idle`：cli，判条件，然后 `sti; hlt; cli` 循环；kernel_exception_handler 对 vector 32 不分 CPL，计数后 EOI 并 return。串口 tick_cpl0=10、tick_idle=10、IDLE delta=10，EXCEPTION 0 次 | 符合。「等待期间」只覆盖 idle：内核在其他时候 IF=0（tick_cpl0 在 idle 前恒为 0），Ring 3 忙等期间 tick 到达的是 CPL3 |
| 7 | 来回改超过两轮报总经理 | 流程项，无法核 | 无法判断 |
通用门槛见 B9。

### 全量 diff 读码（logs/diff-cc80bb3-8fb9198.patch，11 文件 +517/-31）
- interrupts.asm：新增 isr_stub_irq7（push 0、push 39，进 isr_common），stub 表第 39 项改指向它，其余仍是 spurious 桩。
- user_program.asm（+72）：m1_task_a/b 公共体，4 轮，每轮先 SYS_WRITE 一行，再以 SYS_CLOCK_TICKS 忙等 ≥3 tick（不 yield），然后 SYS_YIELD，最后 SYS_EXIT。这段无条件汇编进去，没有测试宏。
- syscall.h：`SYS_CLOCK_TICKS = 48`。
- kernel.c（+264）：AfosSchedCounts；schedule_into_next 成功一次 run[current]++；`#if AGENT_OS_TEST_TIMER_PREEMPT` 改为 `#if AFOS_M1_PREEMPT`（preempt_user_timer 相关的三个函数）；pic_timer_init 去掉宏（重映射主片 0x20、从片 0x28，只开 IRQ0，PIT 分频 65535，约 18.2Hz）；serial_dec、sched_counts_print；scheduler_idle；syscall 48 放在 kernel_syscall_handler 最前面（在 set_frame 之后、其他分支之前），返回 tick，不产生副作用；yield 成功时 yield++；退出后无任务时由原来的 scheduler_halt 改成 scheduler_idle；vector 32 处理：tick++，按 CS&3 记 cpl3/cpl0，idle 时 tick_idle++，**先发 EOI**（outb 0x20,0x20），然后若来自 CPL3 且 PREEMPT=1 就调 preempt_user_timer，成功记 preempt，否则记 preempt_noready；vector 39：OCW3 读 ISR（outb 0x20,0x0B; inb 0x20），bit7 为 0 记 spurious、不发 EOI，否则记 irq7 并发 EOI；新增 `#if !AGENT_OS_TEST_RING3` 的 m1_launch_two_tasks（tss_init、建 2 个进程、加载 initrd 服务 ELF 作为任务 A、内核链接副本作为任务 B、enter_user_mode），替换原来的 `RING3 ABI READY - launch gated pending TSS` 打印。
- tests：m1-t1-check.sh、m1-spurious-irq7-test.sh 为新增；stage7-test.sh 改了 1 行（见 C）。
- tools：外层加 --variant，并拒绝环境里的 AFOS_M1_VARIANT/AFOS_M1_PREEMPT（退出 2）；内层第 5 个参数是 variant，nopreempt 时导出 KERNEL_CFLAGS_EXTRA=-DAFOS_M1_PREEMPT=0；build-env/build-info 记 variant、preempt、env_build；run-m1-uefi.sh 默认运行时长 20→60s，run_dir 改成跟镜像目录走。README 补了 T-M1-1 一节。
读码要点：
- IF：中断门 0x8E/0xEE 进入时清 IF；内核只在 scheduler_idle 的 `sti;hlt` 打开 IF，hlt 被唤醒后，中断返回再执行 cli，所以内核其余代码都在 IF=0 下运行。
- 来自 CPL3 的 tick 经 iretq 恢复用户 RFLAGS。
- EOI 在切换之前发；单 CPU、IF=0，不会重入。
- 栈和寄存器：isr_common 保存全部 GPR 到 ExceptionFrame；preempt 用 exception_to_syscall 存当前帧，再把下一任务的帧写回同一个栈帧，经 iretq 进入。两个任务共用 kernel_stack_top（同一 TSS rsp0），这在单核、不重入的前提下成立。
- tick 是 64 位，18.2Hz 下不存在溢出问题。
- 非法 syscall：末尾打印 `SYSCALL denied - unknown number`，返回 -38。48 在 enum 里唯一，`grep … | uniq -d` 为空（logs/B6-code.txt）。
- 改动面：stage7 那一处以外，现有测试一个没改；内核改动都与本任务直接相关。另外有两处会影响所有构建：pic_timer_init 现在对测试宏构建也生效；默认路径加了 tss_init 和两任务启动，所以 stage7 期望要变（见 C）。B8 回归用来查这两处的影响。
结论：基本符合；5a 方案是否获批，无法判断。

## B2 构建复现与对照组 —— 符合
logs/B2.txt、B2-freeze-diff.txt。t1a、t1b 两个克隆：HEAD=8fb9198，porcelain 0，无 build/。默认、nopreempt 构建都 rc=0；a 原地重建、a 断网 nopreempt、b 断网默认也都 rc=0。
- 默认镜像 `caeeb29059e934d1e9768f3828171cb0b837076ab780413bd9b3d9e9e0741676`：a/b/冻结一致，6 个文件全同。
- nopreempt 镜像 `8f7c4a6a5909942992fd6987087e9a4dd036a1c66f0b3d604169710c498cf6d8`：a/b/冻结一致，6 个文件全同。
- 冻结目录：默认 287 条、nopreempt 44 条，`sha256sum -c` 全 OK；没有清单外文件，也没有缺的；可写位 0；自身哈希 9088c2d7…、901ba372… 都能在 FREEZE-HASHES.txt 里找到。
- 环境开关：`AFOS_M1_PREEMPT=0` 和 `AFOS_M1_VARIANT=nopreempt` 都 rc=2，消息「…in the environment are not accepted」；`--variant=bogus` 也 rc=2。环境里设 `KERNEL_CFLAGS_EXTRA=-DAFOS_M1_PREEMPT=0`，被 env -i 清掉，产物仍是 caeeb290（默认）。
- 对照有效性：
  - BOOTX64.EFI、initrd.bin 相同；kernel.elf 98096 字节对 93864 字节。
  - nm 只有默认里多出 exception_to_syscall、preempt_user_timer、syscall_to_exception 三个符号；.user_text、.user_data 完全相同；.rodata 差 64 字节；strings 只差 `config preempt=on/off`，以及这三个函数名。
  - 按函数统计的反汇编行数，只有 kernel_exception_handler 不同（109 对 91 行），其余函数行数相同；后续节偏移整体平移。
  - 预处理后的 kernel.c 只差 preempt 函数、vector 32 里的 preempt 分支和两处 on/off 字符串。
  - 编译期宏开关，是独立构建。结论：只差开关。

## B5 断言有效性与变异检验 —— 符合（有次要的「判空为 0」路径）
logs/B5-mutation.txt（全部在 /tmp 副本上做）：
- 真实 nopreempt 镜像串口用 nopreempt 模式判：30+ 次全部 PASS（B4）。
- 默认日志用 nopreempt 模式判：rc=1「config line preempt=off missing」。nopreempt 串口用 default 模式判：rc=1（见 B4 补充）。
- 删掉 SCHED counts 行：rc=1。末行 preempt 改成 0：rc=1。run[2]=1：rc=1。删掉任务 B 行：rc=1。追加 EXCEPTION observed：rc=1。删掉 IDLE 行：rc=1，但没有提示消息（pipefail + set -e 静默退出）。
- 阈值改高：preempt≥17 时 rc=1；idle≥11 时 rc=1。
- 发现 1：只检查**末行**的 preempt，首行 preempt 改成 0 仍 PASS（实际两行同源，影响小）。
- 发现 2（「永远通过」路径）：`field()` 取不到值时为空串，bash `[[ "" -eq 0 ]]` 为真。nopreempt 模式下 counts 行里没有 preempt 字段，或者写成 `preempt=`，都判 PASS。还有，把默认日志的 config 行改成 off、再把 preempt 改成 0，也 PASS：判定完全依赖日志自报，没有核对镜像。
- CRLF 能正确处理；缺文件 rc=1；无参数 rc=2。set -euo pipefail 已设置，grep -c 用了 `|| true`。

## B3 启动 —— 符合
`run-m1-uefi.sh`（TCG，默认 60s）：默认镜像跑 5 次，串口 md5 全是 8e5c68ff…，与冻结 boot-clean-a/b 的 3 份相同；nopreempt 跑 5 次全是 f3f9cfa8…，与冻结的 3 份相同。日志头字段齐全：accel=tcg、qemu_version 10.0.13、ovmf_code 路径和 sha256、ovmf_vars、package_claim（标明可伪造）、image sha256、kernel_build_info、完整 qemu_cmd、qemu_exit=124、runtime_report、kernel_version_serial。命令行里没有 -icount、-rtc（run-m1-uefi.sh 第 22–26 行；m1-t1-check.sh 只读日志，不启动 QEMU）。
默认串口摘录：
```
AFOS-M1 config preempt=on
M1 scheduler READY tasks=2 preempt=on
M1 TASK A iter=0 … M1 TASK B iter=3   （ABABABAB）
SCHEDULER idle - all tasks exited
SCHED counts: tick=16 tick_cpl3=16 tick_cpl0=0 tick_idle=0 preempt=16 preempt_noready=0 yield=8 spurious=0 run[1]=13 run[2]=13
IDLE tick check: start=16 end=26 delta=10
SCHED counts: tick=26 tick_cpl3=16 tick_cpl0=10 tick_idle=10 preempt=16 … run[1]=13 run[2]=13
```
nopreempt：tick=24 tick_cpl3=24 preempt=0 yield=8 run[1]=5 run[2]=5；IDLE start=24 end=34。
各数字的含义（自己推的，与串口里其他行对得上）：
- tick：vector 32 处理函数里的计数。
- tick_cpl3：任务运行期间到达的 tick。
- preempt：其中切换成功的次数。默认 16 = tick_cpl3，因为到最后一个任务退出时，另一个任务一直 READY（noready=0）。
- yield=8：4 轮 × 2 个任务。
- run 总和 26 = 1（首次启动）+ 16（preempt）+ 8（yield）+ 1（退出后切换）；串口里 `G2 CR3 switch OK` 正好 25 行。nopreempt：1+8+1=10，CR3 9 行。
- tick_cpl0/tick_idle=10：内核只在 idle 时 IF=1。
- nopreempt 里 tick_cpl3=24 = 8 次等待 × 3 tick：没有抢占时，等待串行进行。默认是 16，因为两个任务的等待相互重叠。

## B4 阈值稳定性（含「计数完全确定」疑点）—— 符合；确定性是设计结构造成的，不是伪造
统计（logs/stability.tsv、stability-summary.txt；每次都完整走 run-m1-uefi + m1-t1-check）：
| 组 | 次数 | 通过 | tick | tick_cpl3 | preempt | yield | load1 最大 |
|---|---|---|---|---|---|---|---|
| TCG 默认 | 30(+B3 5) | 全部 | 26 | 16 | 16 | 8 | 0.79 |
| TCG nopreempt | 10(+5) | 全部 | 34 | 24 | 0 | 8 | 10.35 |
| 负载 4×yes | 15 | 15 | 26 | 16 | 16 | 8 | 6.13 |
| 负载 12×yes（主机 8 核） | 15 | 15 | 26 | 16 | 16 | 8 | 16.1 |
| -icount shift=0/1/2/4 | 3 次/档 | 全部 | 26 | 16 | 16 | 8 | — |
| -icount shift=8 | 3 | 3 | **27** | **17** | **17** | 8 | — |
| shift=6、10，sleep=off，默认 | 各 1 | 通过 | **27** | **17** | **17** | 8，次序 **BABABABA**，run[1]=14 | — |
| shift=6、10，sleep=off，nopreempt | 各 1 | 通过 | **35** | **25** | 0 | 8 | — |
| shift=12、14 | — | — | QEMU 拒绝：「icount: Invalid shift value」 | | | | |
（各组的最小、最大、中位数都相同，只有带粗体的几组例外。load1 出现过 29、57 的尖峰，来自盒子上其他进程，不是我的负载；这些时段的计数同样没变。）

疑点核实：
1. 谁、何时打印：第一行 SCHED counts 由 `scheduler_idle()` 打印，时机是最后一个任务 SYS_EXIT 之后、schedule_into_next 失败时（kernel.c 退出分支）；IDLE 行和第二行 counts，在 idle 循环里 `tick - start >= 10` 首次成立时打印一次。之后内核留在 idle 一直收 tick（-d int 里 90 秒内有 1596 次 CPL0 的 v=20），QEMU 由 timeout 结束。
2. 测试何时结束：任务结束靠固定结构：每个任务 4 轮，每轮等到 tick 增量 ≥3（读数次数不固定，-d int 里 int 0x80 有约 12 万次，正好说明读多少次不由设计决定）。idle 段在增量 =10 时打印。所以 tick_idle、delta 恒为 10，是退出条件决定的；tick_cpl3 是「8 次 ≥3 tick 的等待」在两个任务间重叠后的结果。
3. 为什么不随负载变：PIT 周期 54.9ms；两个 tick 之间，任务要做的事（打印 18 字节，再 syscall）只用微秒级。宿主变慢几倍，tick 和程序事件的先后次序也不变，计数是这个次序的确定函数。tick 来自 QEMU 虚拟时钟（TCG、不开 icount 时随宿主时间走），不是确定性计时。证据：用 -icount shift=8，或 sleep=off 改变每个 tick 内能执行的指令数之后，计数变成 17/27/25，次序变成 BABA（启动阶段 A 还没打印就被抢占了一次）。所以计数是实测的，不是写死，也不是软件推出来的。
4. N 阈值和确定性计数的关系：N=3 写在用户程序里（M1_WAIT_TICKS），是退出条件；检查脚本不判每轮的增量，只判「没有 WAIT TIMEOUT」。脚本阈值是 preempt≥1、tick_idle≥10、run≥2 等，都远低于观测值，或者正好等于退出条件（idle 10）。这些阈值不依赖观测值 16。
5. 证明了什么：tick 确实在到达（不到达等待就不会结束，2^32 次读数后会打印 TIMEOUT）；CPL3 的 tick 确实引起了任务切换。-d int 里 16 个 CPL3 v=20，IP 在 0x400298（A）和 0x187298（B）之间严格交替；nopreempt 也有 24 个 CPL3 tick，但 run 只有 5/5。没有证明的：宿主时间尺度上的抢占频率（结构决定，与宿主无关）。
6. g2-irq 的 TCG 窗口问题（27ms 循环短于 54.9ms）：本任务改用 tick 计数等待，问题已经消失（全部组次 0 次 TIMEOUT）；D1 修好后，idle 期间 tick 照样到达。g2-irq 用例本身没改，仍然失败（见 B8）。

## B5（补充）
真实 nopreempt 串口用 default 模式判：rc=1（config 行不匹配）；用 nopreempt 模式判：PASS。default 和 nopreempt 两个日志做 compare：rc=1「verbatim lines differ」。

## B6 syscall 48 —— 符合
enum 0–9、16–26、32–34、40–47、48，没有重复（logs/B6-code.txt）。用 48 的只有 user_program.asm 两处 `mov eax,48`。处理在 kernel_syscall_handler 最前面，只读。CLOCK_TICKS 返回值单调递增的证据：等待能结束，说明读到的增量 ≥3；-d int 里 int 0x80 约 12 万次；串口里没有逐次打印读数（报告自己也写了「未逐行打印」）。非法编号打印 `SYSCALL denied - unknown number`，返回 -38（代码在 kernel.c 末尾；本轮没有在 Ring 3 实际调用非法编号，只读了代码）。Ring 3 特权指令相关的现有用例（stage7、g2 系列等）在 B8 回归里结果与基线相同。没有专门测 Ring 3 执行 `int $39`：IDT 第 39 项 DPL=0，按架构 Ring 3 执行会触发 #GP，这一条是读码判断。
结论：符合（非法编号实测、Ring 3 int $39 未测）。

## B7 伪中断 39 —— 符合（有已知限制）
读码：vector 39 用 OCW3 读 ISR（`outb(0x20,0x0B); inb(0x20)&0x80`），bit7 为 0 记 spurious、不发 EOI；为 1 记 irq7 并 EOI。实测 `m1-spurious-irq7-test.sh`（测试宏镜像）rc=0，输出 `selftest returned spurious=1`，没有 EXCEPTION，之后继续到 idle（spurious=1）。对照组 `raising vector 40` → `EXCEPTION observed vector=0xff`。注意：spurious 桩统一 push 255，所以对照只能证明「走了未处理向量的路径」，日志里看不出到底是 40。
限制：软件 `int $39` 时 IRQ7 被屏蔽、ISR=0，只走到了伪中断分支，「真实 IRQ7 → EOI」分支没有被执行过；从片伪中断（IRQ15，向量 47，按规则要给主片发 EOI）没有处理，因为从片全被屏蔽，当前不会出现。规格原文对伪中断没有明确要求（T-M1-1 条目里没有），所以这一条属于额外项。用真实机制触发伪 IRQ7 的可行性：QEMU 的 i8259 只在 IRR 被撤销时才产生伪中断，monitor 没有现成的 inject 命令；要做就得写一个探针，在 IRQ 拉起后、INTA 之前撤销请求，或者用 `-d` 配合 gdbstub 改 PIC 状态。可行但麻烦，本轮没做。

## B8 回归 —— 符合
脚本 <临时目录>/r4/reg.sh（复制 regression-run-each 的做法，每项 timeout 300，总上限 90 分钟），无 build/ 的干净克隆 8fb9198，19:55:54 至 20:01:54（UTC+8），在 B4 全部做完之后串行运行，期间没有其他 QEMU。结果 95 项：86 过、9 败（g2-irq、g6-virtio-net-rx/tx、g6-virtio-block、g7-native-block-write/flush/read、g7-native-journal-recovery、journal-commit-recovery）。与 round5 基线、T-M1-0 冻结基线、送审方冻结的 t1 回归逐项比较，通过/失败完全一致（logs/regression-seq-nobuild.tsv、B8-compare.txt）。stage7 在新断言下通过：round5 跑的是旧代码加旧断言，结果同样是 rc=0，两者口径不同，见 C。

## B9 通用门槛
| 门槛 | 判定 | 依据 |
|---|---|---|
| 1 提交+sha256 | 符合 | 219b42f、8fb9198；两个冻结 SHA256SUMS 全 OK |
| 2 干净目录复跑 | 符合 | B2 |
| 3 完整命令、两次一致 | 符合 | B3：5+5 次字节一致 |
| 4 日志保留 | 符合（抽查） | 冻结目录 logs/ 有旧候选目录，保留了 SUPERSEDED |
| 5 不改期望 | 部分符合 | 一处期望变更（C 判定合理） |
| 6 许可证 | 无法判断 | 新文件 SPDX 都是 MIT；LICENSE 只在规格仓库本地未推送的 599e2da 上（见 T-M1-0 r4 A8） |
| 7 无人名 | 符合 | 作者 afos-codex；grep 无命中（沿用 A7 的方法，本分支 diff 也查了，0 命中） |
| 8 日志头 | 符合 | B3 |
| 9 被拒类断言配对照组 | 符合 | nopreempt 对照、vector 40 对照、AFOS_M1_PREEMPT 被拒（同一动作用 --variant 能成功）。**报告自查表把门槛 9 写成「TCG」，属于张冠李戴**（次要） |
| 10 冻结与提交号 | 部分符合 | 只靠 chmod，加 FREEZE-HASHES.txt；版本行 build=8fb9198 |
| 11 默认单一镜像 | 符合 | 默认命令只产出一个镜像；nopreempt 是规格第 4 条要求的「另一个冻结构建」，由 --variant 显式选择，输出到另一个目录，规格允许 |

## B10 自述未验证项的建议
| 项 | 建议 | 理由/实测 |
|---|---|---|
| KVM | 接受为已知限制 | 规格限定 TCG |
| 其他 OVMF | 接受为已知限制 | 日志头以 sha256 标识固件 |
| .deb 签名链 | 接受为已知限制 | 固件身份只认 sha256 |
| sti;hlt 竞态 | 接受（已实测佐证） | 条件在 IF=0 下判断，`sti;hlt` 靠中断影子保证原子；objdump 看到 101c18 sti、101c19 hlt、101c1a cli；-d int 里 1596 次 CPL0 v=20，IP 都是 0x101c1a（hlt 之后），持续到 90 秒，没有睡死 |
| 软件 int $39 不是真伪 IRQ7 | 接受为已知限制（建议 M1 之后补） | 见 B7；真实 IRQ7 和从片路径都没有执行过 |
| 每轮 tick 增量没有逐行打印 | 接受 | 等待条件由代码保证；-icount 下次序变了仍然 0 TIMEOUT |
| ExitBootServices 后残留 LAPIC 定时器 | 接受（已实测） | loader 和内核都不碰 LAPIC；整段 -d int 里只出现 v=20 和 v=80 两种向量，内核阶段（CS=0x18/0x2b）没有其他向量，也没有 EXCEPTION、spurious 计数 0。只限本 OVMF 版本 |

## C stage7 期望变更（8fb9198）—— 判定：合理的期望变更
- diff 全文就是 stage7-test.sh 第 9 行：旧 `grep -Fq 'RING3 ABI READY - launch gated pending TSS'`，新 `grep -Eq '^SCHED counts: .* run\[1\]=[1-9][0-9]* run\[2\]=[1-9][0-9]*'`。
- 旧断言为什么不再成立：219b42f 在 `#else`（非 RING3 宏）分支里，把那行打印换成了 `m1_launch_two_tasks()`，所以内核里已经没有这句话了。
- 四格（logs/C-stage7-cells.txt，两个无 build/ 的新克隆）：fe3e3cd：旧行 1 次、新断言 0 行，stage7 rc=0；8fb9198：旧行 0 次、新断言 2 行，rc=0。与报告一致。
- run[] 的语义：run[i] 只在两处增加，一是 schedule_into_next 切换成功（yield、preempt、exit 后），二是 m1_launch_two_tasks 首次进入任务时（只加给首个任务）。idle 路径和初始化不会增加；只要 run[2]≥1，就说明至少切换到 B 一次。弱点：run[1]≥1 靠首次启动就能满足，而且断言不检查 preempt。不过它只用来替代「launch gated」，目的是证明「默认构建真的启动了两个 Ring 3 任务」，这一点够用；抢占由 m1-t1-check 负责。
- 第二证据（-d int）：16 个 CPL3（cs=0x2b）的 v=20，IP 在 0x400298 和 0x187298 之间交替；12 万次 CPL3 的 int 0x80，分布在两个地址段（0x40029x 约 4.99 万次，0x18729x 约 7.09 万次），对应两个任务。
- 理由：旧断言检查的是一个已被规格批准移除的「尚未启动」状态；新断言检查的是同一测试想表达的「Ring 3 被调度」。只动了 1 行，对照四格有区分力，其余 IDT/KERNEL C OK 断言保持不变。

## B11 仍存在的缺陷与疑点
1. 【次要】m1-t1-check.sh 在字段缺失时取空值，被当成 0：nopreempt 模式下 counts 行没有 preempt 字段，或写成 `preempt=`，都判 PASS；preempt 只检查末行；删掉 IDLE 行时静默失败，没有提示消息。证据：logs/B5-mutation.txt。
2. 【次要】伪中断对照组打印的是 vector=0xff，证明不了到达的是 40；真实 IRQ7 → EOI 分支和从片伪中断都没有覆盖（B7）。
3. 【次要】报告自查表把门槛 9 写成「TCG」，和门槛原文（对照组）对不上（T-M1-1-REPORT.md 自查表）。
4. 【次要】规格 5a「方案交总经理」，没有看到批准记录（B1）。
5. 【次要】irq7 计数不打印；`SYSCALL denied` 和 Ring 3 int $39 这两项没有实测（B6）。
6. 【信息】计数在 TCG、本机条件下由结构决定，复跑字节一致是预期的；换了 icount 会变（17/27），断言照样通过，所以不构成缺陷。
7. 【沿用 T-M1-0 r4】未跟踪头文件可以改变产物而不出 -dirty（重要）；门槛 6 的 LICENSE 不在被审分支上。

## 没来得及核的
真实 PIC 伪 IRQ7 探针；Ring 3 执行非法 syscall 和 `int $39` 的实测；KVM。-d int 原始追踪约 160MB，没有拷进 logs（在 <临时目录>/r4/dint-*.log，摘要见 logs/dint-*-summary.txt）。

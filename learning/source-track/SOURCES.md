# 外部内核清单

更新时间：2026-09-14。这里记录“为什么读、读到哪里、跑到什么程度”，不把仓库的宣传语当成已验证事实。

## 主线仓库

### 1. MIT xv6-public

- URL：https://github.com/mit-pdos/xv6-public
- 本次阅读基线 commit：eeb7b415dbcb12cc362d0783e41c3d1f44066b17（2026-09-14 临时 checkout）
- 架构：旧版 32 位 x86；C、x86 汇编；Unix V6 风格
- 许可证：MIT
- 学习用途：第一份完整纵向样本。它把启动、分页、进程、调度、用户态、syscall、文件系统和 Shell 放在一个仍可逐段追踪的代码库里。
- 首读文件：bootasm.S、bootmain.c、entry.S、main.c、proc.c、vm.c、trap.c、syscall.c、exec.c、fs.c、sh.c
- 上游状态：x86 版本已停止维护，MIT 后续主线转向 RISC-V；因此只把它当完整概念样本，不当我们的目标架构。
- 本机验证：已在临时目录克隆；现代 Ubuntu/GCC 需要处理旧脚本权限/换行和旧指针代码的编译告警，兼容调整后可以在 QEMU 进入 xv6 Shell。这个结果不等于上游在所有现代工具链上开箱即用。

### 2. AxiomX-OS

- URL：https://github.com/ChairSleeper/AxiomX-OS
- 本次阅读基线 commit：338f109e5cfe1f7b96bcc648299f2a84087b0658（2026-09-14 临时 checkout）
- 架构：x86_64；C、NASM；自写 BIOS 四阶段启动器
- 许可证：GPL-2.0（以仓库 LICENSE 为准）
- 学习用途：第二份硬件样本。重点观察实模式、保护模式、长模式、E820、PML4、IDT、PIT、堆和调度在现代 64 位上的组织。
- 首读文件：boot/stage0/boot.asm、boot/stage1/pm_entry.asm、boot/stage2/pm_setup.asm、boot/stage3/longmode.asm、kernel/entry.asm、kernel/kernel.c、kernel/pmm.c、kernel/vmm.c、kernel/idt.c、kernel/sched.c
- 明确缺口：用户态、syscall、ELF loader、文件系统和进程隔离仍在计划中；我们会在自研内核阶段补齐。
- 本机验证：已用 WSL 工具构建，并在 QEMU 中看到初始化链和内核 Shell；这只证明当前临时 checkout 的可运行切片。

## 后期对照仓库

### 3. ToaruOS

- URL：https://github.com/klange/toaruos
- 架构/范围：x86_64，完整内核、用户态、驱动和图形界面
- 许可证：NCSA/University of Illinois 许可，复用前必须阅读仓库 LICENSE
- 用途：后期观察一个较完整的独立系统怎样组织 libc、服务和桌面。
- 当前状态：规模和构建依赖明显大于主线，暂不作为第一份逐行阅读对象。

### 4. PatchworkOS

- URL：https://github.com/KaiNorberg/PatchworkOS
- 架构/范围：x86_64、C/汇编、模块化、IPC、VFS 和能力控制方向
- 许可证：MIT
- 用途：对照现代模块边界和能力模型。
- 当前状态：项目仍处于重构/实验阶段，先读设计和局部实现。

### 5. AuraLite-OS

- URL：https://github.com/AlexanderNyr/AuraLite-OS
- 架构/范围：x86_64、BIOS/UEFI、Ring 3、ELF、VFS、网络和 GUI
- 许可证：以仓库当前 LICENSE 为准
- 用途：后期检查我们的功能地图是否遗漏用户态和产品路径。
- 当前状态：代码规模很大，不进入第一轮阅读。

## 备选样本

### SimpleOS

- URL：https://github.com/raintree-technology/SimpleOS
- 架构：32 位 x86；C、汇编；包含进程、虚拟内存、syscall、文件系统和 Shell 的教学纵切
- 许可证：MIT
- 状态：代码量小，但需要交叉工具链；现代主机编译器适配后的运行结果不能代替官方工具链验收。因此只在 xv6 概念不清时选读，不改变主线。

## 选择原则

“完整”指能从启动追到用户程序和 Shell；“现代”指目标硬件是 x86_64/UEFI；“可教学”指能在一次单元内完成读、跑、改、错、复述。主线用 xv6-public 与 AxiomX-OS 分别满足完整性和现代硬件性，最终代码仍由 Agent-First OS 自己实现。

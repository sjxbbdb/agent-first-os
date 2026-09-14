# Agent-First OS 源码驱动课程总纲

## 课程状态

- 当前路线：Source-First Track
- 当前单元：SF-00，源码基线与阅读方法
- 目标平台：现代 x86_64 PC
- 教学平台：WSL + QEMU/OVMF
- 产品内核：自研混合内核，Ring 0 + Ring 3，先单核
- 外部样本：xv6-public（完整纵切）和 AxiomX-OS（现代 x86_64 硬件纵切）

这份总纲规定学习顺序和能力出口。产品区的 M0–M14、G0–G10 是工程实现编号；它们可以作为映射目标，但产品已有的运行证据不等于学员已经掌握。

## 一、学习目标

最终能够独立解释、实现和验证：

- BIOS/SeaBIOS 到内核入口的启动链，并为同一内核增加 UEFI/OVMF 路径；
- C、x86_64 汇编、链接器、ELF、页表、中断和调试工具之间的因果关系；
- 内存、线程、进程、Ring 3、系统调用、IPC、shared memory 和 capability；
- Supervisor、文件/网络/窗口/输入服务和混合内核边界；
- Policy Firewall、一次性授权、紧急暂停、日志、恢复和回滚；
- 远程模型适配、任务上下文、结构化计划、动作验证和断网降级；
- 每一条自研关键代码的输入、状态变化、不变量、失败后果和运行证据。

“理解每一条代码”只对我们自己写的代码作完整要求。外部内核按当前调用链选读，重点是从真实执行中建立迁移能力。

## 二、学习方法

每个源码单元只有一个可观察出口，循环固定为：

~~~text
选择真实行为
  → 找到入口文件和调用链
  → 读前预测
  → 原样运行并保存证据
  → 只改一处
  → 比较差异
  → 注入一个可恢复故障
  → 逐行解释和口头复述
  → 通过 Unit Gate
~~~

理论按需补齐：只有当前行、当前寄存器或当前错误需要它时，才看 C、汇编、体系结构、教材或规范。视频是补充，不是主线；主线证据来自源码、命令、日志和你自己的变体。

每个单元记录统一字段：unit_id、source、commit、architecture、entry_files、call_chain、read_goal、predict、command、observed_output、one_change、explanation、evidence、product_mapping、status。模板见 [source-track/UNIT_TEMPLATE.md](../source-track/UNIT_TEMPLATE.md)。

## 三、源码主线阶段

### SF-00：源码、工具链和版本基线

**入口**：xv6-public、AxiomX-OS 的仓库元数据和目录树。
**动作**：克隆到仓库外，记录 commit、架构、许可证、构建命令和原始错误。
**出口**：能指出下一条调用链，并区分外部样本、学习实验和产品代码。
**映射**：M0 / G0（仅作工程对照）。

### SF-01：xv6 从 BIOS 到 Shell

**首读文件**：bootasm.S、bootmain.c、entry.S、main.c。
**调用链**：

~~~text
bootasm.S → bootmain.c → entry.S → main.c:main()
  → userinit() → scheduler() → initcode.S → exec("/init") → sh.c
~~~

**出口**：能解释启动扇区、磁盘加载、栈准备、C 入口、第一个用户程序和 Shell 的关系；能在 QEMU 中保留启动日志。
**映射**：M1–M4 / G0、G1 的概念准备。

### SF-02：xv6 的内存、进程、syscall 和文件系统

**首读文件**：vm.c、proc.c、trap.c、trapasm.S、swtch.S、syscall.c、exec.c、fs.c、file.c、sh.c。
**动作**：每次只追一条链：地址空间、时钟调度、用户 syscall、文件读写。
**出口**：能把用户指令经过陷阱、内核检查、服务函数再返回用户态的全过程画出来；能完成一处参数或权限变体。
**映射**：M5–M9 / G1–G4 的机制准备。

### SF-03：AxiomX 的 x86_64 BIOS 硬件链

**首读文件**：boot/stage0/boot.asm、stage1/pm_entry.asm、stage2/pm_setup.asm、stage3/longmode.asm、kernel/entry.asm、kernel/kernel.c。
**再读**：E820.c、pmm.c、vmm.c、idt.c、isr.c、pit.c、sched.c。
**出口**：能解释 real mode → protected mode → long mode、E820、PML4、IDT、PIT 和 round-robin scheduler 的状态变化。
**明确边界**：该样本目前停在内核 Shell，没有完整 Ring 3、syscall、ELF loader 和文件系统；这些由我们的后续实验补齐。
**映射**：M1–M7 / G1–G2 的 x86_64 迁移。

### SF-04：自研内核的启动、内存和异常骨架

**代码边界**：只在 learning/projects/ 中重写最小片段，再与 product/kernel/ 的实现对照。
**动作**：自写 Stage 1/Stage 2、ELF64 加载、版本化 BootInfo、IDT、E820、物理页分配和页表。
**出口**：能从空目录重建一个可启动内核，并能解释每个入口和失败分支。
**映射**：M1–M6 / G0–G2。

### SF-05：Ring 3、syscall、进程和 IPC

**动作**：实现静态 ELF64 用户程序、独立地址空间、用户指针检查、exit/wait、同步 endpoint、shared memory 和最小 capability 表。
**故障题**：非法 syscall、非法用户指针、死循环、伪造句柄、撤销后使用、超时和资源泄漏。
**出口**：能证明一个用户进程出错时，内核和其他进程仍保持不变量。
**映射**：M7–M9 / G2–G4。

### SF-06：Supervisor、系统服务和设备

**动作**：让 Supervisor 作为第一个用户态进程，按 manifest 启动并监控文件、进程、网络、窗口、输入和 virtio 服务。
**出口**：服务崩溃后能回收 capability、重启服务、保留日志并让其他服务继续运行。
**映射**：M9–M12 / G4–G6。

### SF-07：Policy Firewall、恢复和 Agent Runtime

**动作**：实现 L0–L3 风险等级、一次性 token、Trusted Input、任务进程组、暂停/恢复、事务日志、Context Collector 和远程 Model Adapter。
**出口**：远端模型只能提交结构化计划；本地内核和策略决定最终权限；断网、模型错误或 agent 崩溃时系统仍可维护。
**映射**：M11–M14 / G7–G10。

### SF-08：UEFI、SMP 和产品化

**动作**：把同一个 ELF64 内核接到 UEFI/OVMF；处理 GOP、ACPI、ExitBootServices、真实设备和 SMP。
**出口**：能说明教学 BIOS 路径与产品 UEFI 路径的共同 BootInfo 契约和不同故障边界。
**映射**：M10–M14 / G5、G10 及后续真实硬件路线。

## 四、阶段之间怎样迁移

每完成一个源码单元，必须留下三种产物：

1. 外部源码阅读记录：文件、函数、调用链和事实；
2. 自己的最小实验：源码差异、构建命令和运行证据；
3. 迁移说明：哪些机制保留、哪些接口重写、哪些设计不照搬。

只有第三项明确，才算学会“从读代码到做系统”。产品区已有的 G 证据只用于说明工程现状，不能替代这三项。

## 五、能力等级

| 等级 | 表现 |
|---|---|
| L0 | 能识别术语和文件，但不能预测运行 |
| L1 | 能按调用链复现并解释已见输出 |
| L2 | 能独立改一个参数/接口并定位普通错误 |
| L3 | 能从空目录重建小模块、注入故障并给证据 |
| L4 | 能迁移到新架构、设备或权限约束，并评审设计 |

源码单元通常达到 L2 才进入下一个单元；进入自研内核、capability、恢复和 Agent Runtime 阶段要求 L3。安全边界的设计评审要求 L4。

## 六、旧学习记录的处理

C 变量、完整程序、编译、ELF 观察课件和记录不删除，统一视为历史前置。它们只在当前源码行确实需要时补课。历史入口见 [notes/INDEX.md](../notes/INDEX.md)，学习方式迁移记录见 [notes/lesson-log/2026-09-14-source-mode-migration.md](../notes/lesson-log/2026-09-14-source-mode-migration.md)。

当前唯一入口是 SF-00，不再按“第几天”预设学习进度。

# Agent-First OS 最小学习主线

这份文件把 `CURRICULUM.md` 的 15 个阶段转换成可以执行的观看计划。原则是：**每阶段一门主课、一份权威资料、一个本仓库产物**。视频只负责降低入门门槛，真正的掌握以自己的代码、QEMU 运行证据、故障注入和口头解释为准。

不要把所有链接从头刷到尾。表格中的“必看”是进入该阶段前必须完成的内容；“选看”只在遇到具体卡点时打开；“跳过”是为了避免把时间花在与当前目标无关的内容上。

当前教学优先采用源码和小实验。只有当一个主题需要连续演示、不同角度讲解或规范背景时，才安排网课；网课仍然必须和本仓库的源码练习、复述和迁移题绑定。

## 当前学习入口

先完成 [第一次课：程序、变量与赋值](../lessons/day-01-c-basics.md)。诊断中尚未掌握的指针、栈和工具使用会在相应前置知识讲清后再安排。下表是后续资料目录，不是当前作业；课程阶段编号与总纲有差异时，以 `CURRICULUM.md` 为准。

## 先确定观看顺序

```text
翁恺 C（选定章节）
  → 李忠 x86 汇编（实模式到保护模式）
  → NJU ICS 2023（机器级、x86-64、链接、调试、I/O）
  → OSTEP + NJU OS（操作系统概念）
  → MIT 6.828/JOS Labs 1–5（x86 内核实验）
  → 我们自己的 BIOS/UEFI、内核、用户态和 Agent Runtime
```

如果 C 语法基础很弱，翁恺课程和尚硅谷课程二选一，不能两套完整观看。若中文课程的某一段解释不清，再用 OST2 或 Intel 手册查证，不要同时开启更多主线。

如果你对二进制、CPU、内存和 I/O 完全陌生，可在阶段 0 与阶段 1 之间插入哈工大《计算机组成原理》BV1Xu411r7Vz 的少量预习：只看系统简介、基本组成、总线、主存、I/O 概览、指令系统和 CPU 结构；跳过流水线、控制器细节和考研习题。

## 阶段到资料的详细分配

| 阶段 | 必看主课与范围 | 权威资料/源码 | 本阶段跳过 | 进入下一阶段前的产物 |
|---|---|---|---|---|
| 0 工具链 | 翁恺《C 语言程序设计》：开发环境、基本类型、表达式、控制流、函数、数组/字符串、指针、结构体、文件、编译运行；只看能完成练习的章节 | GCC freestanding、GNU `ld`、NASM、QEMU 官方文档 | C++、Java、GUI、算法竞赛专题 | 能从命令行构建 freestanding 小程序，提交工具版本和一次故障复盘 |
| 1 C 与 ELF | 翁恺：指针、数组、结构体、函数指针、内存布局、文件；NJU ICS W2 C 拾遗、W10 链接与加载 | `readelf`/`objdump` 实际输出；ELF 规范性说明 | 完整编译原理课程、自己写编译器 | ELF 段解析器、栈帧图、链接地址实验 |
| 2 x86-64 | B站《2024 零基础 x64 汇编》BV12M4m1o7f6：01–15（寄存器、内存、进制、MOV、算术）；再看 NJU ICS W6 数据机器表示、W7 x86-64/内联汇编、W8 调试 | OST2 Arch1001；Intel SDM Vol.1 体系结构概览 | ARM、Windows 汇编语法、逆向专题 | C 函数与汇编对应实验，能解释 System V AMD64 调用约定 |
| 3 BIOS 启动 | 李忠《x86 汇编语言：从实模式到保护模式》：优先 P1–P21、P23、P25–P26、P30–P34、P40、P43–P45、P51（数制、寄存器、寻址、NASM、复位、硬盘、MBR、调试、保护模式）；MIT JOS Lab 1 的 PC bootstrap、bootloader、GCC calling convention | MIT 6.828 Lab 1；QEMU/GDB | 课程中的 32 位成品 OS 直接复制、Bochs/VirtualBox 安装和重复工具课 | 自写 Stage 1/Stage 2，能定位魔数、磁盘读取和模式切换故障 |
| 4 ELF、分页、内存 | NJU ICS W14 虚拟存储；OSTEP Address Spaces、Address Translation、Paging/TLB；MIT JOS Lab 2 Memory Management | OST2 Arch2001 的物理/虚拟内存章节；Intel SDM Vol.3 分页 | 文件系统、网络、SMP | ELF64 加载器、页表实验、版本化 `BootInfo` |
| 5 中断、特权、syscall | NJU OS：系统调用与 Shell、xv6 代码导读、设备/I/O 相关讲次；OST2 Arch2001 的 Ring 0/3、IDT、中断、syscall、I/O | MIT JOS Lab 3 User Environments；Intel SDM Vol.3 | 先不看 Linux syscall 全量源码 | Ring 3 文字程序、非法指针和非法 syscall 故障样本 |
| 6 进程、线程、调度 | NJU OS：进程、地址空间、上下文切换、调度；OSTEP Processes、CPU Scheduling；MIT JOS Lab 4 的 MP、round-robin、fork 部分 | OSTEP 进程 API 和调度模拟器 | 多核优化、实时调度、容器编排 | 单核抢占式调度器、`exit/wait`、资源回收 |
| 7 IPC 与并发 | NJU OS 并发全段：线程库、互斥、同步、并发 bug；OSTEP Locks、Condition Variables、Concurrency；MIT JOS Lab 4 IPC 与时钟抢占 | JOS IPC 代码；必要时阅读 seL4 IPC 概念 | 复杂无锁算法、分布式一致性 | 同步 endpoint、超时/取消、ping-pong、负向测试 |
| 8 文件系统与持久化 | NJU OS：文件 API、FAT/UNIX、可靠性、xv6 文件系统；OSTEP Files and Directories、FS Implementation、FSCK/Journaling；MIT JOS Lab 5 FS/spawn/shell | OSTEP Crash Consistency；QEMU 磁盘快照 | 先不实现完整 POSIX、网络文件系统 | 最小 VFS、日志/快照、崩溃恢复演示 |
| 9 设备与驱动 | NJU OS：存储、I/O 模型、驱动；NJU ICS W9 I/O；阅读 virtio-block/net 所需章节 | Virtio 规范、QEMU 设备文档、ACPI 入口 | 真实 NVMe、Wi‑Fi、显卡驱动 | virtio 设备服务、超时、设备消失和降级测试 |
| 10 混合内核 | NJU OS：POSIX、Microkernel、Exokernel、Unikernel 对比；精读本仓库 `product/docs/DESIGN_BOOK.md`；用 xv6/JOS 对照 Ring 0/Ring 3 边界 | seL4 capability/IPC 文档；MINIX 多服务器文档 | 不为“像 Linux”而阅读 Linux 全树 | 架构决策记录、服务依赖 DAG、Ring 0 最小化清单 |
| 11 安全与恢复 | OST2 Arch2001 特权/中断复习；OSTEP Security：Authentication、Access Control、Cryptography；精读本项目 L0–L3 和 token 设计 | seL4 capability；JSON Schema 2020-12 | 先不做多用户、Secure Boot 全套、复杂密码协议 | capability 表、一次性 token、可信确认、紧急暂停和回滚测试 |
| 12 网络与远程模型 | MIT JOS Lab 6 Network Driver 只看接口和设备思路；NJU OS 网络/设备相关讲次按需查看 | Virtio-net；Model Adapter 的结构化请求/响应协议 | 本地大模型部署、训练模型、复杂分布式 Agent 框架 | 用户态网络服务、断网处理、远程模型 mock adapter |
| 13 输入、桌面、任务生命周期 | NJU OS 键盘/GPU/I/O 相关内容按需查看；复习本仓库任务生命周期图和 Supervisor 设计 | QEMU 虚拟键盘/鼠标资料；窗口和输入服务接口文档 | 先不追求完整桌面特效和所有应用兼容 | 窗口级上下文、键鼠注入、任务进程组、暂停/恢复/审计 |
| 14 产品整合 | 不新增主课；回看本计划中出现过的卡点章节和 NJU OS 总结 | Intel、UEFI、ACPI、QEMU、virtio 规范；全仓库 Gate | 不再添加新课程或新架构目标 | BIOS→UEFI、单核→SMP、内核→服务→Agent 的完整验收 |

## B站优先清单与使用方式

### C 语言：只选一套

- **首选：浙大翁恺《C 语言程序设计》**。B站可搜索课程名或 BV1dr4y1n7vA；适合零基础，重点看变量、控制流、函数、数组、指针、结构体、文件和编译运行。翁恺官方主页与中国大学 MOOC 课程介绍可作为课程范围核验。
- **替代：尚硅谷《C语言零基础入门教程》**，BV1qJ411z7Hf。需要更慢语速时选它，仍只看上述主题。

### 汇编与启动：中文主课 + 官方补位

- **主课：李忠《x86汇编语言：从实模式到保护模式》**，BV1xE411N74T。优先看 P1–P21、P23、P25–P26、P30–P34、P40、P43–P45、P51；它适合写 BIOS Stage 1/Stage 2，但主要是旧式 32 位保护模式，长模式、syscall 和 UEFI 必须用 OST2、Intel SDM 和我们的实验补齐。
- **入门预热：2024 零基础 x64 汇编**，BV12M4m1o7f6。只看前 15 集建立寄存器、内存和指令直觉，不把它当完整 OS 课程。
- **辅助演示：子牙《如何用纯汇编写一个操作系统》**，BV1Jh41177ba。用来激发兴趣和观察启动链，不作为规范依据，也不直接复制其代码。

### 操作系统：中文概念主线 + x86 实验

- **中文概念：南京大学蒋炎岩《操作系统》**，B站搜索课程名即可。按本表指定的讲次观看；课程较硬，不建议在 C 和汇编之前直接开始。
- **x86 实验：MIT 6.828/JOS**。B站搬运版本不固定，优先使用 MIT 官方 Lab 页面和源码；它承担我们的 BIOS、分页、用户环境、抢占、IPC、文件系统和网络对照。
- **代码走读：xv6 视频**。只在对应阶段阅读进程、虚拟内存、syscall、文件系统；当前 MIT xv6 主线是 RISC-V，所以不把它当 x86 实现教材。

## 国外资料什么时候介入

只有在下列情况才打开国外主课：中文视频缺少对应主题、版本与我们的目标硬件冲突、或需要权威规范确认。

1. 阶段 2：OST2 Arch1001 作为 x86-64 汇编的硬核补位；
2. 阶段 3–8：MIT 6.828/JOS Labs 1–5 作为 x86 实验主参考；
3. 阶段 3、6–8、11：OSTEP 只读指定章节和运行对应模拟器；
4. 阶段 4–5、9、14：Intel SDM、UEFI、ACPI、QEMU、virtio 直接查规范；
5. 阶段 10–13：seL4、MINIX、JSON Schema 和协议文档用于设计对照。

## 按掌握情况推进

每次只学习一个已经具备前置知识的小单元：老师先讲解 → 学员预测或复述 → 亲手练习 → 老师反馈。需要补讲时停在当前知识点，掌握后再进入下一步。基础概念课可以先在对话中回答，编程实验再保存源码和运行证据。

不预设每天或每周的任务量，不估算阶段或总课程的完成时间，不设置观看、阅读和实践的小时数或比例。课程只规定学习顺序与掌握标准。

当视频讲解与我们的目标冲突时，先暂停观看，记录差异，再以 Intel/UEFI/virtio 规范和实际 QEMU 输出为准。任何课程页面可能更新、下架或是搬运版本，开始学习时要重新确认 BV 号、章节标题和版权状态。

## 资料可信度标记

| 标记 | 资料类型 | 用法 |
|---|---|---|
| A | 官方规范、大学课程主页、作者本人课程 | 可作为事实和课程范围依据 |
| B | 高校课程的 B站搬运或成熟教材讲解 | 可用于理解，关键细节需回到 A 类资料 |
| C | 个人 UP 的手写 OS 演示 | 只作直觉和调试示范，不复制设计或代码 |

当前主线优先级是：A 类规范/课程确定事实，B 类中文视频建立直觉，C 类视频提供演示，最后由我们的代码和 Gate 证明真正掌握。

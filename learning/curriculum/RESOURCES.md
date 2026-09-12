# Agent-First OS 学习资料

资料只作为课程的支撑，不替代自己的实现和验收。外部课程的答案、作业解答和受限制材料不直接复制到项目仓库。

阶段化的观看顺序见 [STUDY_PLAN.md](STUDY_PLAN.md)；本文件保留完整候选资料，方便遇到具体卡点时查阅。

## 推荐主线

- [Nand2Tetris 官方课程](https://www.nand2tetris.org/)：从逻辑门、CPU、汇编器、VM、编译器到教学 OS，适合补齐硬件到软件的整体直觉。官方提供在线 IDE 和项目工具；课程材料带有非商业使用限制，公开仓库时只提交自己的实现。
- [Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)：围绕虚拟化、并发、持久化和安全组织的免费教材，配有模拟器和作业。适合阶段 3 的理论主线。
- [MIT 6.1810 / xv6](https://pdos.csail.mit.edu/6.1810/2025/xv6.html)：官方 xv6 源码和教材，适合对照进程、虚拟内存、文件系统、线程和 syscall。当前 xv6 主线是 RISC-V，因此只作为概念和内核组织参考。
- [MIT 6.828 x86/JOS](https://pdos.csail.mit.edu/6.828/2018/)：更贴近 x86、QEMU、GDB、PC bootstrap、启动器、保护模式和 JOS 实验，适合阶段 1–7 的对照学习。

## 视频与网课

### B站优先主线（只选指定部分）

- [浙大翁恺《C语言程序设计》](https://www.bilibili.com/video/BV1dr4y1n7vA/)：阶段 0–1 首选。只看开发环境、类型/表达式、控制流、函数、数组/字符串、指针、结构体、文件和编译运行；不需要把整套课程刷完。
- [尚硅谷《C语言零基础入门教程》](https://www.bilibili.com/video/BV1qJ411z7Hf/)：翁恺课程听不懂时的替代，只二选一，不与翁恺整套并行。
- [2024 零基础 x64 汇编](https://www.bilibili.com/video/BV12M4m1o7f6/)：阶段 2 只看前 15 集，建立寄存器、内存、进制和基本指令直觉；长模式和系统编程仍以 OST2/Intel 为准。
- [李忠《x86汇编语言：从实模式到保护模式》](https://www.bilibili.com/video/BV1xE411N74T/)：阶段 3 的中文启动主课，重点看数制、实模式、A20、GDT、保护模式和加载 C；它主要覆盖 32 位，不能替代 x86_64/UEFI 资料。
- [从零开发操作系统](https://www.bilibili.com/video/BV18K411w7Z2/)：阶段 3–5 选看启动、ELF、C/汇编联合编程和 Ring 0/Ring 3 章节。页面标注为搬运，内容只作直觉参考。
- [南京大学《操作系统》蒋炎岩](https://www.bilibili.com/video/BV1Cm4y1d7Ur/)：阶段 6–11 按 `STUDY_PLAN.md` 指定主题观看；课程较硬，不作为零基础第一门课。
- [清华《操作系统原理》](https://www.bilibili.com/video/BV1uW411f72n/)：阶段 3 的中文概念补课，只按进程、内存、文件、I/O 和系统调用主题查漏。
- [用 x86 汇编语言实现 64 位操作系统](https://www.bilibili.com/video/BV1gM4y1V7Ac/)：阶段 2、5 的辅助课，按分页、中断、特权级、多任务主题选看；不直接复制其实现。
- [南京大学《计算机系统基础（一）》](https://www.bilibili.com/video/BV19J411T7rq/)：阶段 1–2 的桥梁课，只看数制/补码、ISA/指令、过程调用、数组/指针、汇编与链接加载。
- [南京大学《计算机系统基础（二）》](https://www.bilibili.com/video/BV1Xx411E7qn/)：阶段 4 的补充，只看 Cache、虚拟存储、地址转换和存储保护。
- [南京大学 ICS 2023 实验课](https://www.bilibili.com/video/BV1vj411H7N6/)：阶段 1–5 的实验桥梁，选看 C 拾遗、NEMU、机器级表示、x86-64、调试、链接加载、中断和虚拟存储周次。

B站课程的 BV 号、章节和版权状态可能变化；开始学习时重新打开页面核对。高校课程搬运和个人 UP 视频统一作为 B/C 类资料，硬件事实回到 Intel、UEFI、QEMU 和 virtio 规范。

- [OpenSecurityTraining2 Arch1001](https://p.ost2.fyi/courses/course-v1:OpenSecurityTraining2+Arch1001_x86-64_Assembly+2024_v1/about)：x86‑64 汇编、寄存器、栈、调用约定、位运算和 Intel 手册阅读，适合阶段 1–3。
- [OpenSecurityTraining2 Arch2001](https://p.ost2.fyi/courses/course-v1:OpenSecurityTraining2+Arch2001_x86-64_OS_Internals+2024_v1/about)：执行模式、Ring 0/3、MSR、IDT、中断、syscall、分页和端口 I/O，适合阶段 2、6、7。
- [MIT OCW 6.828 课程与实验](https://ocw.mit.edu/courses/6-828-operating-system-engineering-fall-2012/)：包含 PC 硬件、x86 汇编、QEMU/GDB、启动、虚拟内存、线程、进程、文件系统和崩溃恢复；[Lab 1 PDF](https://ocw.mit.edu/courses/6-828-operating-system-engineering-fall-2012/8b840ca51c24004623bc1b85859ce0d8_MIT6_828F12_lab1.pdf) 作为启动阶段练习参考。
- [Berkeley CS162 讲座资源](https://people.eecs.berkeley.edu/~kubitron/cs162/lectures.html)：操作系统、并发、内存、文件和系统设计的课程讲义与多媒体资源。
- [Nand2Tetris 官方视频与项目](https://www.nand2tetris.org/)：硬件到软件的完整项目路径；适合作为并行补修，不替代 x86_64 启动链。
- [Nand2Tetris Coursera 课程](https://www.coursera.org/learn/build-a-computer)：官方课程的网课版本；平台证书和付费状态以课程页面为准。

## 中文辅助课程

- [南京大学 GIST 操作系统课程](https://gist.nju.edu.cn/course-os/)：现代操作系统概念、API、进程、调度、内存、同步、文件和设备实验。
- [清华 uCore OS Labs](https://github.com/chyyuu/os_kernel_lab)：中文分阶段内核实验，从启动、中断、内存、线程、用户进程到文件系统；架构和工具较旧，只作机制对照。
- [中国大学 MOOC：北京交通大学《操作系统》](https://www.icourse163.org/course/NJTU-1003245001?tid=1003475004)：理论与工程方法补充，覆盖启动、syscall、进程、线程、同步、分页、设备和文件系统。
- [中国大学 MOOC：华中科技大学《操作系统原理》](https://www.icourse163.org/course/HUST-1003405007)：理论、算法、Linux/Windows 案例和开源 OS 分析。

视频用于预习和形成直觉，不能替代我们自己的小产物、代码解释和故障验收。不同课程的架构、工具和作业版本可能变化，实际学习时以课程当前页面为准。

## 体系结构和启动规范

- [Intel 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)：阶段 2、4、6、7、14 的权威参考；重点读 Volume 1 和 Volume 3 的系统编程章节。
- [UEFI Forum Specifications](https://uefi.org/specifications)：UEFI、GOP、启动服务和 ACPI 规范入口。产品路径以 UEFI 为主。
- [ACPI Specification](https://uefi.org/specs/ACPI/6.6/)：阶段 5、10、14 的硬件发现和电源管理参考。
- [QEMU 官方文档](https://www.qemu.org/docs/master/system/)：系统仿真、串口、GDB stub、快照和设备配置。
- [Virtio 规范](https://docs.oasis-open.org/virtio/)：阶段 10 的 virtio-block 和 virtio-net 参考。

## C、汇编和工具链

- [GCC Freestanding Environments](https://gcc.gnu.org/onlinedocs/gcc/Freestanding-Environments.html)：内核编译选项和 freestanding 约束。
- [GNU ld Linker Scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)：阶段 1、4、6 的链接布局。
- [NASM 文档](https://www.nasm.us/doc/)：阶段 1、4、7 的汇编器参考。
- [OSDev Wiki Tutorials](https://wiki.osdev.org/Tutorials)：交叉编译、启动器和设备开发的实践索引。它是社区资料，可能混有旧的 BIOS/32 位假设，不能替代 Intel、UEFI 和设备规范。

## 隔离、组件和 Agent 接口

- [seL4 Documentation](https://docs.sel4.systems/)：即使最终采用混合内核，也可用来学习 capability、用户态组件和 IPC。
- [MINIX 3 Documentation](https://www.minix3.org/doc/)：用于对比多服务器、故障隔离和自愈系统。
- [JSON Schema 2020-12](https://json-schema.org/draft/2020-12)：工具参数、结果和计划 schema 的约束基础。
- [Model Context Protocol Specification](https://modelcontextprotocol.io/specification/2024-11-05/basic)：远程模型工具/资源协议的参考，不要求原样采用。

## 资料使用原则

1. 规范回答“硬件或协议规定了什么”；
2. 教材回答“为什么需要这个机制”；
3. 我们自己的代码回答“这个 OS 选择怎样实现”；
4. QEMU、GDB、串口和故障注入回答“当前实现是否真的工作”。

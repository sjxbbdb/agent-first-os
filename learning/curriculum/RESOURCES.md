# 源码主线资料库

资料服务于当前源码单元，不构成必修播放列表。使用顺序由 [源码单元执行表](STUDY_PLAN.md) 决定：先读、先跑，遇到具体缺口再打开对应资料。

## 一、外部内核源码

| 仓库 | 架构与范围 | 许可证 | 主线用途 | 当前验证边界 |
|---|---|---|---|---|
| [MIT xv6-public](https://github.com/mit-pdos/xv6-public) | 32 位 x86，C/汇编，启动、内存、进程、用户态、syscall、文件系统、Shell | MIT | SF-01/SF-02 的完整概念样本 | 已在临时目录兼容调整后跑到 QEMU Shell；现代 GCC 不是开箱即用 |
| [AxiomX-OS](https://github.com/ChairSleeper/AxiomX-OS) | x86_64，C/NASM，四阶段 BIOS、E820、PML4、IDT、PIT、调度、内核 Shell | GPL-2.0 | SF-03 的现代硬件样本 | 临时 checkout 已构建并在 QEMU 启动；用户态、syscall、ELF、文件系统尚缺 |
| [ToaruOS](https://github.com/klange/toaruos) | x86_64，完整内核、用户态、驱动和 GUI | NCSA/University of Illinois | SF-06/SF-08 后期对照 | 规模和依赖较大，未作为当前构建基线 |
| [PatchworkOS](https://github.com/KaiNorberg/PatchworkOS) | x86_64，模块化、IPC、VFS、能力控制 | MIT | 对照服务边界和能力模型 | 实验性重构，按局部源码阅读 |
| [AuraLite-OS](https://github.com/AlexanderNyr/AuraLite-OS) | x86_64，BIOS/UEFI、Ring 3、ELF、VFS、网络、GUI | 以仓库 LICENSE 为准 | 后期检查功能地图 | 规模很大，不进入第一轮 |
| [SimpleOS](https://github.com/raintree-technology/SimpleOS) | 32 位 x86，进程、虚拟内存、syscall、文件系统、Shell | MIT | xv6 概念的可选补充 | 需交叉工具链，主机编译器适配不能替代正式验收 |

许可证只说明阅读和复用条件；产品代码默认独立重写。需要复用第三方代码时，先更新 [LICENSE_MATRIX.md](../../product/docs/LICENSE_MATRIX.md)。

源码仓库的 URL、commit、构建命令和实际状态集中记录在 [source-track/SOURCES.md](../source-track/SOURCES.md)。克隆目录放在 WSL 用户目录或其他仓库外路径。

## 二、按需视频和教材

### C、汇编、工具链

- [浙大翁恺 C 语言程序设计](https://www.bilibili.com/video/BV1dr4y1n7vA/)：只在当前 C 语法阻塞源码时看变量、函数、指针、结构体或编译章节。
- [2024 零基础 x64 汇编](https://www.bilibili.com/video/BV12M4m1o7f6/)：只看寄存器、内存、进制和基本指令的前 15 集。
- [李忠 x86 汇编：从实模式到保护模式](https://www.bilibili.com/video/BV1xE411N74T/)：只在 SF-01/SF-03 遇到 BIOS、A20、GDT 或保护模式问题时观看指定分 P。
- [南京大学计算机系统基础](https://www.bilibili.com/video/BV19J411T7rq/)：只看当前需要的数制、调用约定、汇编、链接加载或虚拟存储章节。
- [OpenSecurityTraining2 Arch1001](https://p.ost2.fyi/courses/course-v1:OpenSecurityTraining2+Arch1001_x86-64_Assembly+2024_v1/about)：x86-64 寄存器、栈和调用约定的硬核补位。

### 操作系统和内核实验

- [MIT 6.828 x86/JOS](https://pdos.csail.mit.edu/6.828/2018/)：启动、分页、用户环境、抢占、IPC、文件系统和网络的 x86 对照实验。
- [Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)：只读当前问题对应的虚拟化、并发、持久化或安全章节。
- [南京大学操作系统](https://www.bilibili.com/video/BV1Cm4y1d7Ur/)：需要中文概念解释时，按单元指定主题观看。
- [清华 uCore OS Labs](https://github.com/chyyuu/os_kernel_lab)：用于中文实验对照；工具和架构较旧，不复制实现。
- [Nand2Tetris](https://www.nand2tetris.org/)：硬件到软件的并行补修，不替代 x86_64 启动链。

网课每次都要记录实际章节和观看目的；观看完成不等于 Unit Gate 通过。

## 三、硬件和协议规范

- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)：模式、特权级、中断、syscall、分页、APIC 和 I/O。
- [UEFI Specifications](https://uefi.org/specifications)：UEFI 启动、GOP、启动服务和 ExitBootServices。
- [ACPI Specifications](https://uefi.org/specs/ACPI/6.6/)：RSDP、表发现和电源管理入口。
- [QEMU System Documentation](https://www.qemu.org/docs/master/system/)：串口、GDB stub、快照和虚拟设备。
- [Virtio Specifications](https://docs.oasis-open.org/virtio/)：virtio-block、virtio-net、virtqueue 和输入设备。
- [GCC Freestanding Environments](https://gcc.gnu.org/onlinedocs/gcc/Freestanding-Environments.html)：内核编译约束。
- [GNU ld Linker Scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)：内存布局和链接地址。
- [NASM Documentation](https://www.nasm.us/doc/)：汇编语法和输出格式。

规范回答“硬件或协议要求什么”，源码回答“这个实现怎样做”，运行证据回答“当前 checkout 是否真的工作”。

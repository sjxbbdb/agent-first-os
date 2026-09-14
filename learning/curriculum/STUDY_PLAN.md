# 源码单元执行表

这份文件把总纲转换为下一步可执行的源码阅读顺序。它不是视频播放列表，也不要求按日期或固定时长推进。

## 执行规则

每个单元只做一件可以观察的事：

1. 选定仓库和 commit；
2. 只读入口文件及其下一跳；
3. 写预测；
4. 运行原始版本；
5. 只改一处；
6. 运行改动版本；
7. 做一个可恢复的故障实验；
8. 保存证据并口头复述。

老师会根据你的复述和证据决定继续、补课还是回到上一跳。视频只在当前单元的“按需补位”栏出现时观看。

## 当前单元

| 字段 | 内容 |
|---|---|
| ID | SF-00 |
| 目标 | 建立 xv6-public 和 AxiomX-OS 的可重复源码基线 |
| 入口 | [SF-00 讲义](../lessons/SF-00-source-baseline.md) |
| 交付 | commit、目录计数、版本/许可证记录、五道预测题 |
| 状态 | 未开始 |

## 主线单元

| ID | 只读源码 | 运行观察 | 一次改动 | 出口 |
|---|---|---|---|---|
| SF-00 | 仓库 README、LICENSE、Makefile、目录树 | clone、版本记录、原始 build 结果 | 改一个构建参数或输出标记 | 能说明两个样本的角色和边界 |
| SF-01 | xv6-public：bootasm.S、bootmain.c、entry.S、main.c | SeaBIOS/QEMU 启动到内核入口 | 改一个 marker 或加载参数 | 能画 BIOS→C 入口链 |
| SF-02a | xv6-public：vm.c、mmu.h | 页表建立、CR3 切换和地址转换 | 改一个映射范围 | 能解释一次 page fault |
| SF-02b | xv6-public：proc.c、swtch.S、trap.c | 时钟、yield、scheduler、上下文切换 | 改一个 quantum/状态转移 | 能解释线程从运行到就绪 |
| SF-02c | xv6-public：syscall.c、sysproc.c、usys.S | 用户程序发起 syscall 并返回 | 增加或拒绝一个 syscall 参数 | 能画 Ring 3→Ring 0→Ring 3 |
| SF-02d | xv6-public：exec.c、fs.c、file.c、sh.c | ELF 用户程序、文件读写和 Shell | 改一个路径或错误分支 | 能说明文件层到磁盘层 |
| SF-03 | AxiomX-OS：boot/stage0–stage3、kernel/entry.asm、kernel.c | x86_64 BIOS 启动到内核 Shell | 改一个阶段 marker | 能对照 32 位与 64 位模式 |
| SF-03m | AxiomX-OS：E820.c、pmm.c、vmm.c、idt.c、pit.c、sched.c | 内存、中断、时钟和调度初始化 | 改一个页/频率/命令参数 | 能解释硬件状态变化 |
| SF-04 | 我们的 learning/projects 实验 | 自写 Stage 1/2、ELF64、BootInfo、IDT | 从空目录重建一个最小切片 | 能独立定位启动故障 |
| SF-05 | 我们的 kernel 与用户程序 | Ring 3、syscall、进程、IPC、shared memory | 加一项权限或负向测试 | 能证明越权被拒绝 |
| SF-06 | 我们的 Supervisor 和服务 | 服务启动、崩溃、重启、virtio | 注入一次服务故障 | 能说明故障不越界 |
| SF-07 | Policy、Registry、Agent Runtime | token、Trusted Input、任务恢复、mock 远程模型 | 改一个风险或后置条件 | 能证明模型不能自授予能力 |
| SF-08 | UEFI loader、ACPI、SMP 相关源码 | OVMF、GOP、ExitBootServices、单核→多核 | 改一个 BootInfo 字段或启动模式 | 能解释产品启动边界 |

SF-02a 至 SF-02d 是同一份 xv6 源码的四个小出口；不要求一次打开所有文件。

## 第一个调用链

第一轮只追这条链，不阅读整棵仓库：

~~~text
xv6 bootasm.S
  → bootmain.c
  → entry.S
  → main.c:main()
~~~

完成后再接上：

~~~text
main()
  → userinit()
  → scheduler()
  → initcode.S
  → exec("/init")
  → sh.c
~~~

每个箭头都要填写“输入、状态变化、下一跳、可观察证据”。模板见 source-track/UNIT_TEMPLATE.md。

## 构建和运行入口

### xv6-public

上游 Makefile 面向旧 x86 ELF 工具链。先运行原始命令并保存错误：

~~~bash
cd ~/agent-os-references/xv6-public
make
make qemu-nox
~~~

如果现代 GCC 因脚本换行、执行权限或旧指针告警失败，不要直接删除错误；把失败当成 SF-00 的证据，老师会带你区分环境问题、编译问题和内核问题。

### AxiomX-OS

~~~bash
cd ~/agent-os-references/axiomx-os
make clean
make
make run
~~~

README 要求的交叉编译器如果不在 PATH，先记录缺失工具和 Makefile 期待的变量；不要用未经说明的主机编译器结果替代正式验收。我们已经在临时 checkout 看到它能在 QEMU 进入内核 Shell，但每次学习仍以当前 commit 的实际日志为准。

## 网课和教材的介入点

| 触发条件 | 只看什么 | 看完怎样验收 |
|---|---|---|
| 看不懂 bootasm.S 的寄存器或寻址 | x86 汇编入门中寄存器、寻址、栈的片段 | 对当前三条指令写前后寄存器预测 |
| 看不懂分页代码 | OSTEP 地址空间/分页指定章节或 OST2 Arch2001 对应小节 | 画一次虚拟地址到物理地址的页表路径 |
| 看不懂调度 | OSTEP Processes、CPU Scheduling 指定章节 | 改一个状态并解释调度顺序 |
| 看不懂 syscall | MIT JOS Lab 3 或 NJU OS 系统调用片段 | 追一条用户 syscall 到返回 |
| 看不懂文件日志 | OSTEP FS/Journaling 指定章节 | 注入一次截断并说明恢复证据 |
| 需要 UEFI 事实 | UEFI、ACPI、Intel、QEMU 官方规范 | 用规范条款和实际日志各证明一次 |

B站课程仍然可以优先使用中文版本，但每次必须写明视频标题、实际章节和它解决的当前源码问题。看完不提交复述和迁移题，状态仍是“未开始”。

## 单元出口检查表

- [ ] 入口文件和 commit 已记录
- [ ] 读前预测已保存
- [ ] 原始运行命令和输出已保存
- [ ] 只改了一处并能解释差异
- [ ] 至少一个故障或负向路径已观察
- [ ] 能脱离源码复述调用链
- [ ] 写出与 Agent-First OS 的保留/重写/不照搬项
- [ ] 课后记录已链接到 notes/INDEX.md 和 notes/PROGRESS.md

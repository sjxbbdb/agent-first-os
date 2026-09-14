# SF-00：源码基线与阅读方法

## 这节课的出口

完成后，你应该能：

- 说清楚我们为什么同时阅读 xv6-public 和 AxiomX-OS；
- 在 WSL 中把两个仓库克隆到学习区之外；
- 记录每个仓库的 commit、架构、许可证和构建状态；
- 从目录树中指出下一条要追的调用链；
- 在运行前写出自己的预测，而不是先看答案。

这不是“把两个仓库下载完就算学会”。下载只是建立可重复的观察对象。

## 我们要解决的真实问题

一个能够独立启动的操作系统，至少要能追出下面这条纵向链：

~~~text
固件/启动器
  → C 内核入口
  → 内存和中断
  → 线程/进程
  → 用户态
  → syscall
  → 文件或 Shell
~~~

xv6-public 提供一份小而完整的旧 x86 样本；AxiomX-OS 提供一份现代 x86_64 的 BIOS 与硬件样本。两者都是教材，不是我们产品的源代码。

## 先做环境和版本记录

在 VS Code 的 WSL 终端执行：

~~~bash
mkdir -p ~/agent-os-references

git clone --depth 1 https://github.com/mit-pdos/xv6-public.git ~/agent-os-references/xv6-public
git clone --depth 1 https://github.com/ChairSleeper/AxiomX-OS.git ~/agent-os-references/axiomx-os

cd ~/agent-os-references/xv6-public
printf 'xv6 commit: '
git rev-parse HEAD
printf 'xv6 files: '
find . -type f -not -path './.git/*' | wc -l

cd ~/agent-os-references/axiomx-os
printf 'AxiomX commit: '
git rev-parse HEAD
printf 'AxiomX files: '
find . -type f -not -path './.git/*' | wc -l
~~~

如果仓库已经存在，不要重复 clone，改用：

~~~bash
git -C ~/agent-os-references/xv6-public status --short
git -C ~/agent-os-references/xv6-public rev-parse HEAD
git -C ~/agent-os-references/axiomx-os status --short
git -C ~/agent-os-references/axiomx-os rev-parse HEAD
~~~

把命令和完整输出保存到 learning/evidence/SF-00-source-baseline.md。不要把第三方仓库复制到 D:/Agent OS/product/。

## 第一次只读哪些文件

先不要打开整棵树。下一单元只读 xv6 的这四个入口：

~~~text
bootasm.S
bootmain.c
entry.S
main.c
~~~

对照调用链：

~~~text
bootasm.S → bootmain.c → entry.S → main.c:main()
~~~

AxiomX 只先看：

~~~text
boot/stage0/boot.asm
boot/stage1/pm_entry.asm
boot/stage2/pm_setup.asm
boot/stage3/longmode.asm
kernel/entry.asm
kernel/kernel.c
~~~

把每个文件的“输入、输出、下一跳”写成一行。暂时不读调度器、文件系统和 Agent Runtime。

## 读前预测题

先独立回答，再运行：

1. BIOS 为什么会从 0x7c00 开始执行启动扇区？
2. 如果 bootmain.c 没有把内核读进预期的物理地址，下一步会出现什么类型的故障？
3. 为什么从汇编跳入 C 前必须准备栈？
4. AxiomX 的 stage3 和 xv6 的 entry.S 都在做模式切换吗？请先写你的猜测和依据。
5. 一个“内核 Shell”是否等于 Ring 3 用户程序？为什么？

## 构建规则

本节先记录克隆和版本，不要求立刻修复所有构建问题。xv6-public 是旧 x86 工具链项目，在现代 Ubuntu/GCC 上可能出现脚本换行、执行权限或旧指针告警；遇到失败时保留原始错误，下一单元再逐项定位。不要为了得到绿色输出而把错误日志删掉。

AxiomX-OS 的 README 提供 make 和 QEMU 入口；实际使用的编译器、参数和输出必须写入证据文件。一次构建成功只证明当前 checkout 的这条路径可运行，不证明仓库的所有功能都已完成。

## 平板笔记

请只记下面四个重点，并各写一句自己的话：

- 【重点】源码阅读从一条可观察调用链开始，不从整棵仓库开始；
- 【重点】外部源码是对照样本，产品代码必须独立记录；
- 【重点】运行前预测、运行后证据和一次改动共同证明理解；
- 【易错】内核 Shell、产品 G0/G10 证据和学员自己通过的学习 Gate 是三种不同状态。

## 交付格式

把下面内容发给老师：

1. 两个仓库的 commit 和目录计数；
2. 五道预测题的答案及理由；
3. 四个 xv6 文件的输入/输出/下一跳表；
4. 平板笔记中的四条重点；
5. 你遇到的原始错误或运行输出。

老师会据此决定下一单元是先跑 xv6，还是先补 C/汇编/工具链中真正阻塞当前调用链的那一个点。

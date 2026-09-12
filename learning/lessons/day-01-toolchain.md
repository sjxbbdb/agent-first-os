# 第一天：确认工具链，第一次生成 freestanding C 目标文件

## 今天的出口

完成今天后，你应该能回答：

1. 当前命令是在 Windows、WSL 还是 QEMU 客体中执行的？
2. `gcc -ffreestanding` 与普通应用编译有什么区别？
3. `.c`、`.o`、可执行 ELF 三者分别处在构建链的哪一步？
4. 为什么今天只生成目标文件，不急着运行它？
5. `readelf` 能从目标文件中证明哪些事实？

## 第一步：先做入学诊断

不要查资料，直接把答案写进 `learning/notes/day-01-diagnostic.md`，每题写出你的理由：

1. 把十六进制 `0x2A` 转成十进制。
2. 设 `int x = 7; int *p = &x;`，分别解释 `p`、`*p`、`&x` 的含义。
3. 函数调用时，局部变量通常为什么会出现在栈上？画出调用者、被调用者、返回地址的关系。
4. 下面三个 Makefile 目标中，哪个应该先执行，为什么：`all`、`hello.o`、`clean`？
5. 写出 GDB 中“在 `main` 入口断点、运行程序、查看寄存器”的三条命令；如果你不知道 `main` 在哪里，说明你会怎样查。

诊断不是考试。它用于决定后续哪些概念要补讲，不能用分数替代理解。

## 第二步：记录现场工具链

在 WSL Ubuntu 终端执行并保存输出：

```bash
uname -sr
gcc --version | head -n1
nasm -v
ld --version | head -n1
objdump --version | head -n1
readelf --version | head -n1
gdb --version | head -n1
qemu-system-x86_64 --version | head -n1
find /usr/share/OVMF -maxdepth 1 -type f -name '*.fd' -print | sort
```

把输出保存为 `learning/evidence/day-01-toolchain-versions.txt`。这份证据只记录版本，不代表工具已经能正确构建内核。

## 第三步：亲手写一个 freestanding C 小程序

在 `learning/projects/stage-0/day-01-toolchain/` 下创建：

- `hello.c`：只使用整数、指针、循环和结构体，写一个不会调用 libc 的小函数集合；至少包含一个整数函数和一个读取字节缓冲区的函数。
- `Makefile`：提供 `all`、`hello.o`、`inspect`、`clean` 目标；编译时使用 `-ffreestanding -fno-builtin -mno-red-zone -Wall -Wextra -std=c11 -c`，先只生成 `hello.o`。
- `README.md`：记录你选择的函数、每个编译选项的作用，以及你没有提供 `main`/启动入口的原因。

建议先自己写，再用下面的命令检查结果：

```bash
make clean && make
file hello.o
readelf -h hello.o
readelf -S hello.o
objdump -dr hello.o
```

不要把“能被 Linux 直接运行”当成今天的目标；`hello.o` 是给链接器和后续内核入口使用的中间产物。

## 第四步：做一次小故障注入

完成第一次构建后，故意删除 `-ffreestanding` 或把目标文件名写错，重新执行 `make`，保存报错，然后恢复并重新构建。你要在 README 中写清楚：

- 哪一层发现了错误（Make、GCC、链接器还是 shell）；
- 错误信息中哪一行是根因；
- 恢复后用什么命令证明构建回来了。

## 今天提交的证据

提交一个 commit，至少包含：

```text
learning/notes/day-01-diagnostic.md
learning/evidence/day-01-toolchain-versions.txt
learning/projects/stage-0/day-01-toolchain/hello.c
learning/projects/stage-0/day-01-toolchain/Makefile
learning/projects/stage-0/day-01-toolchain/README.md
```

在下一次课前，把以下内容发给老师：诊断答案、`make` 完整输出、`readelf -h` 输出、故障注入前后差异，以及你最不理解的两行 C 或 Makefile。

## 本周主线

本周只围绕阶段 0 展开，不进入 BIOS 启动链：

- 补齐 WSL、Git、Make、GCC、Binutils、GDB、QEMU、OVMF 的基本使用；
- 读懂 C 的指针、数组、结构体、位运算和内存布局；
- 能区分预处理、编译、汇编、链接和加载；
- 用 `readelf`、`objdump` 和 GDB 观察自己的目标文件；
- 完成一个可复现的小构建，并能解释一次失败和恢复；
- 阶段结束时做一次独立变体：不看原文件，新增一个函数并证明它出现在正确的 ELF section 中。

本周不看完整 OS 视频，不复制任何现成 bootloader，也不开始写分页。等这些基础证据通过检查后，下一步才进入 C/ELF 与机器码的对应关系。

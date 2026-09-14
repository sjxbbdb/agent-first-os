# 第三节：第一次观察 ELF Header

## 本节出口

你能够用 `readelf -h` 找到并用自己的话解释：

- 文件是不是 ELF；
- 文件是 32 位还是 64 位；
- 目标机器架构是什么；
- 文件属于什么类型；
- 程序入口地址是什么；
- Section Header 和 Program Header 在文件中的位置由谁记录。

本节先只看 Header，不急着分析每一个 section，也不要求记住所有字段。

## 先建立一个比喻

ELF 可以暂时看成一个“程序容器”：里面有机器代码、数据、符号和加载所需的信息。ELF Header 是容器最前面的说明卡，告诉工具怎样继续寻找这些内容。

请记笔记：

- **【重点】** ELF 是 Executable and Linkable Format 的缩写；
- **【重点】** Header 描述文件整体，告诉工具文件类型、架构、入口和其他表的位置；
- **【易错】** ELF 文件格式和 Linux 操作系统不是一回事；Linux 只是可以加载和运行 ELF 的一种系统；
- **【易错】** `Entry point address` 是装载后最先进入的机器代码地址，不要直接把它等同于 C 的 `main`。普通动态链接程序通常先进入运行时入口，再由运行时调用 `main`。

## 第一个观察命令

在 VS Code 的 WSL 终端执行：

```bash
cd "/mnt/d/Agent OS/learning/projects/stage-0/day-02-compile"
readelf -h hello
```

把完整输出复制保存或发给老师。先只找下面六行，不必一次理解全部内容：

```text
Class:
Data:
Type:
Machine:
Entry point address:
Start of section headers:
```

## 观察练习

看完自己的输出后回答：

1. `Class` 告诉你什么？
2. `Machine` 为什么应该和我们的 x86_64 目标一致？
3. `Type` 如果显示 `DYN`，它和我们刚才看到的 PIE 有什么关系？
4. `Entry point address` 为什么不一定等于 `main` 的地址？
5. 如果把一个 ELF 文件的 Header 破坏，系统还能可靠地加载它吗？为什么？

## 本节笔记

在平板上记录 `【重点】`、六个字段的含义，以及你实际输出中的值。至少画一条关系：

```text
ELF Header → 告诉工具去哪里找 → Section Header / Program Header / Entry Point
```

## 下一步

收到输出、练习答案和笔记后，老师会逐项批改，再决定是否继续 `readelf -S`。如果字段含义还不稳固，会先用更小的例子复习，不会直接进入复杂 ELF 细节。

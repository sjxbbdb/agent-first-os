# 源码主线

这里是外部内核源码的阅读入口。第三方仓库只作为学习样本和对照资料，克隆到仓库外；我们自己的代码放在 learning/projects/，通过 Gate 后才进入 product/。

## 阅读顺序

~~~text
SF-00  建立源码、工具链和版本基线
  ↓
SF-01  xv6-public：从 BIOS 到内核入口，再到 Shell
  ↓
SF-02  xv6-public：内存、进程、syscall、文件系统的完整纵切
  ↓
SF-03  AxiomX-OS：同一条硬件链在 x86_64 上如何实现
  ↓
SF-04  自研内核：把机制缩小并重新实现
  ↓
SF-05  Ring 3、syscall、IPC、shared memory、capability
  ↓
SF-06  Supervisor、系统服务和设备
  ↓
SF-07  Policy Firewall、恢复和 Agent Runtime
  ↓
SF-08  UEFI、SMP 和产品化
~~~

每个箭头代表能力迁移，不代表把上一个仓库复制到下一个仓库。

## 外部源码的位置

默认使用 WSL 用户目录：

~~~bash
mkdir -p ~/agent-os-references
git clone --depth 1 https://github.com/mit-pdos/xv6-public.git ~/agent-os-references/xv6-public
git clone --depth 1 https://github.com/ChairSleeper/AxiomX-OS.git ~/agent-os-references/axiomx-os
~~~

先记录 git rev-parse HEAD，再开始阅读。需要完整历史时去掉 --depth 1。不要把克隆目录放入 product/，也不要把第三方源码直接提交到公开仓库。

## 版本和许可证规则

每一个源码单元都要记录：

- 仓库 URL；
- commit 或 tag；
- 目标架构和工具链；
- LICENSE 类型；
- 本机实际执行过的命令；
- 哪些行为已经运行验证，哪些只是阅读推断。

许可证只决定我们如何阅读和引用，不自动授权把代码复制进产品。产品实现优先由我们独立重写；若以后确实要复用代码，先在 product/docs/LICENSE_MATRIX.md 记录影响。

## 当前阅读纪律

1. 一次只打开调用链中的少量文件；
2. 先写预测，再运行；
3. 一次只改一处；
4. 保留原始输出，不用截图代替命令；
5. 故障实验必须可恢复、可说明；
6. 读到不懂的语法时，回到当前行所需的 C、汇编或硬件知识补齐；
7. 外部源码的结论和我们自己的实现结论分开记录。

更多信息：

- [外部内核清单](SOURCES.md)
- [调用链地图](CALL_CHAINS.md)
- [源码单元模板](UNIT_TEMPLATE.md)
- [当前单元讲义](../lessons/SF-00-source-baseline.md)

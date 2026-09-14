# 源码学习方式迁移记录

## 背景

此前的学习按 C 基础小节和视频顺序推进，记录停在 ELF Header 观察。学员反馈希望直接阅读一份小而完整的内核，边运行边理解真实调用链。

## 本次决定

从 2026-09-14 起，主线切换为 Source-First Track：

- 以 xv6-public 建立完整 OS 纵向地图；
- 以 AxiomX-OS 观察现代 x86_64 BIOS 硬件链；
- 在 learning/projects/ 中独立重写关键机制；
- 通过 Ring 3、IPC、capability、Supervisor 和 Agent Runtime 单元逐步迁移到产品设计；
- 视频和理论改为遇到具体源码缺口时按需补齐。

## 保留内容

C-PRE-01 至 C-PRE-06 课件、平板笔记批改、day-02-compile 实验和已有 evidence 不删除。它们改为历史前置/补课材料，状态仍按当时的实际证据记录。

产品区的 M0–M14、G0–G10 证据也不删除，但不计入学员的 SF Gate。产品证据和学习证据在索引中分开。

## 当前出口

当前唯一单元是 SF-00：源码基线与阅读方法。完成仓库克隆、commit/许可证记录、目录树观察和预测题后，才进入 SF-01 的 xv6 BIOS→Shell 调用链。

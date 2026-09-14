# 学习笔记索引

这里登记源码单元的笔记、课后记录和待复习项。平板原笔记可以留在你的笔记软件中，整理后的摘要放到 lesson-log/；没有同步时，以你发给老师的文字或照片为准。

课后记录的命名和历史边界见 [lesson-log/README](lesson-log/README.md)。

## 当前源码主线

| 单元 | 讲义/记录 | 状态 | 下一步 |
|---|---|---|---|
| SF-00 源码基线与阅读方法 | [讲义](../lessons/SF-00-source-baseline.md) · [课后记录](lesson-log/2026-09-14-SF-00-source-baseline.md) | 未开始 | 克隆仓库并写五道预测题 |
| SF-01 xv6 BIOS→Shell | [调用链](../source-track/CALL_CHAINS.md) | 未开始 | 等 SF-00 Gate |
| SF-02 xv6 机制纵切 | [执行表](../curriculum/STUDY_PLAN.md) | 未开始 | 等 SF-01 Gate |
| SF-03 AxiomX x86_64 | [源码清单](../source-track/SOURCES.md) | 未开始 | 等 SF-02 Gate |
| SF-04–SF-08 自研和产品迁移 | [课程总纲](../curriculum/CURRICULUM.md) | 未开始 | 按阶段解锁 |

当前状态的唯一来源是 [学习进度](PROGRESS.md)。本表只提供导航，不能单独改变状态。

## 历史前置课次

这些记录来自源码主线启用前的 C/ELF 入门。它们全部保留，可在当前源码遇到对应语法或工具问题时补课，但不会阻挡 SF-00：

| 记录 | 状态 | 用途 |
|---|---|---|
| [2026-09-12 lesson-01](lesson-log/2026-09-12-lesson-01.md) | 已复述 | 变量、赋值和顺序执行 |
| [2026-09-12 lesson-02](lesson-log/2026-09-12-lesson-02.md) | 已复述 | main、函数体和 return |
| [2026-09-12 lesson-03](lesson-log/2026-09-12-lesson-03.md) | 已复述 | 编译、目标文件、链接和运行 |
| [2026-09-13 lesson-04](lesson-log/2026-09-13-lesson-04.md) | 已通过 | 完整程序入口与编译前检查 |
| [2026-09-13 lesson-05](lesson-log/2026-09-13-lesson-05.md) | 已通过 | WSL、GCC、返回码和 ELF 类型 |
| [2026-09-14 lesson-06](lesson-log/2026-09-14-lesson-06.md) | 进行中/已暂停 | readelf Header 观察 |
| [第一周第一节笔记批改](lesson-log/week-1-lesson-1-note-review.md) | 已复述 | 早期平板笔记复核 |
| [第一周第二节笔记批改](lesson-log/week-1-lesson-2-note-review.md) | 已通过 | 编译运行笔记复核 |
| [源码模式迁移记录](lesson-log/2026-09-14-source-mode-migration.md) | 已记录 | 说明为什么切换主线 |

## 笔记规则

每个源码单元的记录至少包含：

- 读前预测；
- 指定文件、符号和调用链；
- 原始命令和输出；
- 一处改动及差异解释；
- 故障或负向路径；
- 口头复述和与自研 OS 的映射；
- 老师批改标签和下一步。

模板见 [课堂笔记模板](NOTE_TEMPLATE.md) 与 [源码单元模板](../source-track/UNIT_TEMPLATE.md)。

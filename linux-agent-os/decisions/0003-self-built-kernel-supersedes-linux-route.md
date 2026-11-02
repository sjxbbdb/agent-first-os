# 决定 0003：路线改为自研内核，Linux 线暂停
日期：2026-11-02　决定人：项目负责人确认（经总经理转达）

## 决定内容
1. 路线改为自研内核。Linux 魔改线暂停，原型1（Landlock 任务沙箱）到此收尾。
2. 决定 0001 中「只做 Linux 魔改」一条被本决定取代，不再有效。
3. 决定 0001 中「外部副作用一律人工确认」一条保留，对自研内核同样适用。
4. 决定 0002 中与具体内核无关的原则继续有效：整机默认跑在 microVM 里、开发阶段保留 shell 稳定后去掉、人工确认渠道用 Webhook 加签名 token。这些原则在自研内核上的落地方式待新内核方案重新评估，0002 里涉及 Linux 具体机制的细节不再作为实施依据。

## 理由
项目负责人明确拍板自研内核。本条只记录结论，不重复讨论过程。

## 对已有文档的影响
- `decisions/0001-route-and-side-effects.md`：路线一条被取代，外部副作用一条保留。原文不改，以本决定为准。
- `decisions/0002-microvm-dev-shell-confirm-channel.md`：原则保留，落地方式待重估。原文不改。
- `specs/proto1-landlock-task-sandbox.md`、`specs/proto3-agent-native-taskbox.md`、`tasks/proto1-landlock-tasks.md`：Linux 线暂停，暂不改动正文，仅在任务清单末尾记路线变更。新内核的规格另起文件。
- `prototypes/proto1-landlock/`：已入库的源码、脚本和日志作为历史证据保留，不再推进。

## 仍待办
- 自研内核第一里程碑的规格与任务清单，等总经理给依赖安排后另写。
- Linux 规格中与内核无关、可作为新内核需求沿用的条目，等调研结论出来后单列。

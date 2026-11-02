# Agent OS 规格文档

面向 AI agent 的 Linux 内核改造项目的规格、任务清单与决策记录。

## 目录
- `research/`  调研结论（来自调研组，原文归档）
- `decisions/` 总经理的决定，一条一个文件，编号递增
- `specs/`     规格文档，每份含：目标、范围、接口、风险、验证方法
- `tasks/`     任务清单，每个任务含：负责人、输入、验收标准、依赖

## 当前内容
- `decisions/0001-route-and-side-effects.md` 路线与外部副作用原则（已定）
- `research/round1-agent-os-gaps.md` 第一轮调研：8 个需求缺口与内核机制取舍
- `specs/proto1-landlock-task-sandbox.md` 原型1 规格（草稿）
- `tasks/proto1-landlock-tasks.md` 原型1、原型2 任务清单

## 约定
- 文档与提交信息不含任何个人信息（姓名、邮箱、本机路径里的用户名）。
- 规格文件头部写版本号与日期，改动写进文件末尾的变更记录。
- 项目负责人尚未拍板的事项在规格里标「待拍板」，不当作已定。

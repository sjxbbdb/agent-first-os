# DECISION-0001：Agent Runtime 采用可替换 harness

日期：2026-09-13

## 决策

Agent Runtime 放在 Ring 3，优先基于 **pi agent** 二次改造；对执行循环、结构化工具调用、验证、恢复和日志等能力，按需吸收 OpenAI Codex harness 的成熟设计。若后续验证表明单一框架更合适，可以保留其中一个作为唯一实现，但对外继续使用本项目定义的任务、工具和授权协议。

## 原因

- pi agent 结构较轻，便于逐步替换模型适配、上下文收集和执行器；
- Codex harness 可作为执行可靠性、工具编排和恢复语义的参考；
- 复用成熟运行时能把精力放在 OS 原生能力、capability 和策略闭环上；
- 框架位于用户态，崩溃可以由 Supervisor 回收，不能成为内核信任根。

## 不变量

1. agent framework 不能进入 Ring 0；
2. 框架不能绕过 kernel capability、Policy Firewall 或 Trusted Input；
3. 模型只接收当前任务窗口和明确授权的文件；
4. 离线系统不依赖远端模型或框架启动；
5. 框架可替换而不改变 native syscall/IPC 和任务日志协议。

## 当前状态

这是 M11–M14 的实现基线，当前仅完成架构决策记录。pi agent 与 Codex harness 的具体版本、许可证、适配接口和组合方式将在进入 Agent Runtime 实现阶段时重新核验并记录。

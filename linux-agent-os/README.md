# Agent OS 规格文档

面向 AI agent 的 Linux 内核改造项目的规格、任务清单与决策记录。

## 目录
- `research/`  调研结论（来自调研组，原文归档）
- `decisions/` 总经理的决定，一条一个文件，编号递增
- `specs/`     规格文档，每份含：目标、范围、接口、风险、验证方法
- `tasks/`     任务清单，每个任务含：负责人、输入、验收标准、依赖

## 当前内容
- `decisions/0001-route-and-side-effects.md` 路线与外部副作用原则（已定）
- `decisions/0002-microvm-dev-shell-confirm-channel.md` microVM 默认、开发阶段保留 shell、确认渠道（已定）
- `decisions/0003-self-built-kernel-supersedes-linux-route.md` 路线改为自研内核，Linux 线暂停（已定，取代 0001 的路线一条）
- `decisions/0004-m1-code-base-token-signature-capability.md` M1 的代码基础、令牌签名（Ed25519）、能力撤销（已定）
- `decisions/0005-license-mit.md` 许可证选 MIT（仓库根 LICENSE，整个仓库按 MIT，例外保持 GPL）
- `specs/license-mit-rollout.md` MIT 落地方案与执行记录
- `research/round1-agent-os-gaps.md` 第一轮调研：8 个需求缺口与内核机制取舍
- `specs/proto1-landlock-task-sandbox.md` 原型1 规格（草稿）
- `specs/proto3-agent-native-taskbox.md` 原型3 M-A1 规格（草稿）
- `specs/native-kernel-m1.md` 自研内核第一里程碑规格（v0.12，里程碑内容已确认，T-M1-2 接口已草拟（未定稿），T-M1-3、T-M1-4 待写）
- `research/round2-agent-native-os.md` 第二轮调研：完全给 agent 用的操作系统
- `research/round3-afos-baseline.md` 自研内核基线调研：agent-first-os 现状、第一里程碑差距、Linux 规格复用清单（只读源码，未编译未运行）
- `research/round4-confirm-token-crypto.md` 确认令牌的密码原语调研（已过验收审查；密码原语待项目负责人拍板）
- `research/round5-afos-qemu-baseline.md` agent-first-os 在 QEMU 的实测基线（验收审查有条件通过，仅为基线记录，非 M1 证据）
- `research/round5-run-each-summary.tsv`、`research/round5-key-log-sha256.txt` round5 的逐项汇总表与关键日志哈希
- `research/round6-g2-irq-analysis.md`、`research/round6-g2-irq-key-sha256.txt` g2-irq 根因报告（已复核，TCG 下成立，KVM 未测）与关键文件哈希
- `research/round7-stage-reviews.md`、`research/round7-review-data-*.md`（5 个）、`research/round7-review-logs-sha256.txt` 验收审查对 T-M1-0（四轮）和 T-M1-1 的阶段审查记录（TCG 下的阶段结论，不带入 T-M1-5），日志只记哈希
- `research/round6-g2-irq-review.md`、`research/round6-g2-irq-review-sha256.txt` 验收审查对该报告的复跑记录与日志哈希
- `prototypes/proto1-landlock/` 原型1 源码、脚本、文档与日志（Linux 线已暂停，不含二进制）
- `tasks/native-kernel-m1-tasks.md` 自研内核 M1 任务清单（T-M1-0 到 T-M1-5）
- `tasks/proto1-landlock-tasks.md` 原型1、2、3 任务清单（T1 到 T7）

## 约定
- 文档与提交信息不含任何个人信息（姓名、邮箱、本机路径里的用户名）。
- 规格文件头部写版本号与日期，改动写进文件末尾的变更记录。
- 项目负责人尚未拍板的事项在规格里标「待拍板」，不当作已定。

# 决定 0002：microVM 默认、开发阶段保留 shell、确认渠道
日期：2026-11-02　决定人：项目负责人确认（经总经理转达）

## 决定内容
1. 系统整机默认跑在 microVM 里（Firecracker 这类），VM 内每个任务再套沙箱。
2. 开发阶段保留 shell，稳定后再去掉，改为带外维护模式。注意是「开发阶段保留」，不是第一版就无 shell。
3. 人工确认渠道先用 Webhook 加签名 token。

## 理由
- 依据 `research/round2-agent-native-os.md` 第四节第 2、3、4 条的调研倾向，项目负责人对这三条拍板。
- 其中第 2 条与调研倾向不同：调研倾向是完全不要 shell 和登录，决定改为开发阶段保留，原因是调研自己指出没有调试入口时早期开发会很痛。

## 仍待拍板（本决定不涵盖）
- 基线内核版本（调研倾向 6.18 longterm）。
- VM 内执行不可信代码的子任务是否再加 gVisor（调研倾向加）。
- 确认 token 由谁验证（调研倾向 policyd 在用户态验证，与仓库设计书的内核验证不一致）。
- computer-use 是否降为可选（调研倾向降为可选）。
- 「稳定」的判定标准，以及带外维护模式的具体形态（调研参考 Bottlerocket admin container 思路）。

## 影响的规格与任务
- `specs/proto3-agent-native-taskbox.md`：待拍板表中三项改为已定，升到 v0.2；M-A1 构建分开发配置与稳定配置，用例 U5「串口上无 shell」开发阶段可选、稳定配置再启用。
- `tasks/proto1-landlock-tasks.md`：T5、T6 的验收标准同步修改，升到 v0.4。
- 实施时机：microVM 属于路线阶段 E，M-A1 仍在 QEMU 里验证，不引入 microVM；Webhook 加签名 token 属于阶段 D，M-A1 不实现。

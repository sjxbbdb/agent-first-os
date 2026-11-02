# Round 8：DeepSeek 开源 agent 框架调研（对 agent-OS 的借鉴）

> 入库说明（方案规格，2026-11-02）：内核调研出具的原文，未改动正文。本文是参考外部公开项目的思路提炼，只取设计思路，不引用其代码；没有运行过文中提到的任何原型或框架，结论依据以文内标注（已读、未验证、推断）为准。本文是 M2 以后的参考，不改变 M1 范围。


> 检索时间：2026-11-02（UTC+8）。来源均为本轮实际打开的页面，未打开的标"未验证"。

## 1. 核实结论

- "super agent" 这个名字**不对**。DeepSeek 实际开源的是 **DeepSeek Harness（命令行 `dsh`）**，口号 "Everything is a Plugin"，MIT 许可，目前是 developer preview（README 明说会有破坏兼容性的变更）。
- 官方仓库：https://github.com/deepseek-ai/deepseek-harness （组织 `deepseek-ai`，README 写明 "developed by DeepSeek AI"）
- 官方页面：https://www.deepseek.com/en/harness/
- 文档：https://deepseek-harness.github.io/deepseek-harness/ （本轮未打开，未验证）
- 配套论文：*A Programming Paradigm for Spatiotemporal Composability*，arXiv:2608.25512（cs.PL / cs.SE），v1 提交于 2026-08-26 16:22（UTC+8），92 页；作者单位为北京大学与 DeepSeek-AI。https://arxiv.org/abs/2608.25512
  - 注意：这篇论文讲的是底层框架 **Cordis**（https://github.com/cordiverse/cordis）的形式化理论，**不是一篇 agent 论文**。它把 agent harness 当作动机场景（§1.2.2），案例研究用的是 Koishi 插件生态（§5.3），文中没有 agent 方面的实验。
- 相关第三方论文（不是 DeepSeek 写的）：*The Empire, Long Divided, Must Unite: Architectural Convergence in Three LLM Agent Harnesses*，arXiv:2608.23953。它在源码层面对比了 deepagents、pi 和 dsh。本轮只读了摘要和引言。
- 其他候选：没有找到叫 "super agent" 的 DeepSeek 项目（未验证是否另有同名项目）。

## 2. 架构提炼

| 维度 | dsh 的做法 | 依据 |
|---|---|---|
| 总体 | 每个部分都是 Cordis 插件，包括模型适配器、工具注册表、会话日志，**连 agent 循环本身也是**，都可以通过配置替换。没有享有特权的核心。插件做的"注册"本身是一种 effect，插件卸载时会自动撤销 | `docs/architecture.md`（Cordis 一节） |
| 组合 | profile = 一组有序的 bundle 层，再叠加 `cordis.patch.yml`。可用 `--dump-config` 查看启动后的完整插件树。出厂 profile 有 web/headless/sdk/sdk-minimal/acp | `docs/architecture.md`（Profiles and bundles） |
| Agent 循环 | turn 由 0 到多个 step 组成；一个 step = 一次模型请求加上它调用的工具。钩子是 waterfall 形式：`agent/pre-step` 可以接受、改写或拒绝输入；工具走 `tools/pre-execute → execute → post-execute`。请求发出前会被冻结成不可变的形式 | `docs/architecture.md`（Turn flow） |
| 工具调用 | `ToolDefinition` 必须声明输出 schema，并带调度元数据（timeout、isConcurrencySafe 等）。暴露给模型的 schema 按白名单构造，宿主字段不会泄漏进请求。执行时必须响应取消信号 | `docs/subsystems/tools.md` |
| 沙箱 | `SandboxMode` 有 read-only / workspace-write / danger-full-access 三档，**只管文件副作用**，网络和进程可见性不在这套词汇里。后端分别是 Linux bwrap+Landlock、macOS Seatbelt、Windows 受限令牌。**实际执行程度会作为事实上报：full 还是 partial** | `docs/subsystems/sandbox.md` |
| 权限/审批 | `ApprovalOutcome` 是封闭的四值：allowed-once / rejected / cancelled / unavailable。**fail-closed**：应答方缺失、抛错或返回不合规结果，一律算 unavailable，调用方拒绝执行。每次请求都分配新 id，`approval/asked` 和 `approval/decided` 成对写入审计。每个会话有 ask/never 策略，最终取会话日志里最后一条为准，所以回放时可以重建 | `docs/subsystems/approval.md` |
| 权限预设 | 把沙箱模式和审批策略这两个独立的旋钮打包成具名预设，另外有实验性的 Auto review | `docs/subsystems/permission-presets.md` |
| 状态/记忆 | 只追加的 `SessionEvent` 日志是模型上下文的唯一来源。原则是 **"Model-visible means logged"**：每一次模型请求都必须能从日志重建。fork、resume、遥测、转录都从日志派生。会话格式带版本号，迁移每次只升一级，已经提交的那一代文件永不改写 | `docs/architecture.md`（Session log） |
| 任务恢复 | 失败的 step 会补记缺失的工具结果；未封口的中断尾部由消费者修复；agent 可以冷启动恢复（cold-resume） | `docs/architecture.md`；`docs/subsystems/agent-team.md` |
| 凭据 | 配置里只存引用（环境变量名），值由 provider 持有。每次操作都重新解析，所以凭据轮换后立即生效。`describe()` 的返回结构里根本没有能放值的字段 | `docs/subsystems/credentials.md` |
| 可观测性 | OTel 插件；会话日志通道在上报前先做授权和脱敏；只挂载 OTel 不会分配任何传输通道 | `docs/subsystems/otel.md` |
| 多 agent | 实验性 Agent Team：成员生命周期为 provisioning → active/failed；消息投递到目标 inbox，在最近的 step 边界生效（steer）；目标闲置时会唤起，未加载时会冷恢复；没有发件箱，也不做重发去重 | `docs/subsystems/agent-team.md` |
| Computer use | 实验性功能，通过 Cua Driver 提供方实现；按应用粒度授予同意，可以撤销；只允许一个 provider | `docs/subsystems/computer-use.md` |
| 安全声明 | 未经安全审计。官方建议放在一次性 VM 或容器里跑，并说明沙箱不保证隔离 | `SAFETY.md` |

论文核心（Cordis）：
- **可逆效应（revertible effects）**：上下文上的每一次变换都附带一个逆操作，由运行时持有。组件卸载时按 LIFO 顺序撤销，这就是"时间可组合性"（§3.1）。
- **反应式共效应（reactive coeffects）**：组件声明自己依赖什么；依赖满足了就激活，依赖消失就停用，这就是"空间可组合性"（§3.2）。
- **系统边界**：能独占修改、并且能恢复的位置算边界内，操作可回滚；对外发送数据（emission）不可回滚，只能扣留（withhold，即 output commit 问题）或补偿（compensation）（§6.1）。
- **能力式访问控制**：组件只能访问它声明过的依赖，所以加载时就能审阅它的全部能力；拦截器可以在运行时收窄权限，不需要重载（§6.3）。不可信代码需要外部沙箱，通过桥接方式访问宿主（§6.3）。
- **与 OS 协同设计**：论文主张由 OS 把"组件声明的 coeffect"当作它能触及的全部资源，并把 OS 自身的资源作为 coeffect 提供出来，类似 Wasm 的 imports（§6.7）。这一节和我们的方向直接相关。

## 3. 对 agent-OS 的借鉴（逐条）

### 可借鉴
1. **把能力六分类映射到"边界内 / 边界外"**（论文 §6.1）。"只读"和"可逆"对应边界内，配有逆操作；"本地提交"对应边界内但需要持久化；"外部副作用"对应 emission，只能扣留或补偿。可以用来定义一条状态机规则：外部副作用必须先进入"扣留"态，人工确认后才放出去。这和我们 M1 的人工确认状态机能直接对上。
2. **资源配额 syscall 改成"加载时声明全部 coeffect"**（§6.3、§6.7）。Ring 3 任务在启动时就带上它需要的能力清单（可绑定在 Ed25519 令牌里），内核不再给任何其他可达资源。效果上类似 Wasm imports 或 capability 系统，好处是能在加载时整体审阅，而不是事后发现越权。
3. **每项资源授予都登记逆操作，任务退出时按 LIFO 回收**（§3.1、§6.1 acquisition 阶段）。fd、内存、子任务、配额都按这种方式登记。退出或撤销令牌时自动清理干净，这可以作为内核里任务资源表的设计不变量。
4. **审批结果封闭且 fail-closed**（`docs/subsystems/approval.md`）。我们的确认状态机建议显式区分四种结果：允许一次、拒绝、撤回、不可用。超时或确认方缺失一律按"不可用"处理并拒绝；用一次性 id 把请求和决定配成一对写入审计。
5. **沙箱执行程度作为事实上报**（`docs/subsystems/sandbox.md`）。内核或硬件能力不完整时，返回 `partial`，不要假装隔离是完整的；需要绝对保证的调用方据此拒绝执行。
6. **"Model-visible means logged"，只追加且可回放的日志**（`docs/architecture.md`）。M2 的语义界面里，agent 看到的每一帧观测、发出的每个动作都写进只追加的日志，任务恢复和审计都从这份日志派生。在此基础上还可以补上 dsh 缺的一块：外部可验证性。第三方论文 arXiv:2608.23953 指出三个 harness 都没有"防篡改、外部可校验"的记录。我们已有 Ed25519，可以做哈希链加签名（仅读了摘要，结论未逐节验证）。
7. **凭据只给引用、每次操作才解析、查询接口没有放值的字段**（`docs/subsystems/credentials.md`）。可以直接套用到"读敏感值"这一类能力：agent 只拿到句柄，真正的值只在内核或可信组件里解引用。
8. **多 agent 消息在 step 边界投递**（`docs/subsystems/agent-team.md`）。M2 以后做多任务通信时，可以把"安全点投递"和"冷恢复唤起"做成内核 IPC 的语义。

### 不适用或需谨慎
1. **热替换（HMR）和"一切皆插件"运行在 TypeScript/Node 的单一运行时里**（`docs/architecture.md`；论文 §6.4）。语言层面的访问控制挡不住恶意代码，论文 §6.3 自己也承认这一点。内核里必须靠硬件隔离（Ring 3 加页表），不能照搬。
2. **dsh 的沙箱不管网络和进程可见性**（`docs/subsystems/sandbox.md`），而且未经审计（`SAFETY.md`）。不能把它当作安全模型的参照。我们的"外部副作用"和"原始注入"两类恰恰需要覆盖网络。
3. **论文 92 页的形式化（演算、合流性等，§4）对 M1 来说太重**。只取边界、逆操作、依赖声明这几个概念就够了，不需要实现完整演算。
4. **审批策略只有 ask/never 两档**（`docs/subsystems/approval.md`），粒度比我们的六类能力粗。可以借鉴它的结构，分类沿用我们自己的。
5. **Computer use 是 macOS 上基于像素/无障碍接口的实验方案**（`docs/subsystems/computer-use.md`），和我们 M2 的"无头语义界面"不是同一路线。只有"按应用粒度授予、可撤销"这一点可以参考。
6. **Agent Team 不做重发去重**（`docs/subsystems/agent-team.md`）。内核 IPC 需要更强的投递语义，不能照搬。

## 4. 未验证 / 局限
- 论文只精读了目录、§1.2、§6.1–6.3、§6.7 和结论，§3–§5 的形式化细节没有逐条核对。
- 仓库只读了 README、`docs/architecture.md`、`SAFETY.md` 以及 `docs/subsystems/` 下的 sandbox、approval、permission-presets、tools、agent-team、computer-use、credentials、otel 这几篇，没有读源码实现。
- 第三方论文 arXiv:2608.23953 只读了摘要和引言。

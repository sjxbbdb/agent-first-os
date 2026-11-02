# Round 9：dsh-superagent-prototype（DeepSeek）对 agent-OS 的借鉴

> 入库说明（方案规格，2026-11-02）：内核调研出具的原文，未改动正文。本文是参考外部公开项目的思路提炼，只取设计思路，不引用其代码；没有运行过文中提到的任何原型或框架，结论依据以文内标注（已读、未验证、推断）为准。本文是 M2 以后的参考，不改变 M1 范围。文内的设计文章链接来自转载站，官方原文未确认，保持文内原样标注，没有改成官方链接。


- 仓库：https://github.com/deepseek-ai/dsh-superagent-prototype （MIT；本次读的是 2026-10-31 20:05 UTC+8 的提交 6337fc8，浅克隆）
- 设计文章《Exploring Super-agent Design: Towards a More Autonomous Agent System》（作者署名 DeepSeek Harness 团队）：
  - 原文看起来是发在 X 上的一篇长文（转载站的 URL 里带着 X 帖子 ID 2116829218935857152；转载站说原文是中文）。但我直接打开 `x.com/i/status/2116829218935857152` 返回 404（可能是要登录），所以**官方原文链接没能确认**。
  - 全文是从转载站读的（英文机翻版）：https://haowenji.com/articles/x-2116829218935857152 。另有一篇二手解读：https://agientry.com/en/articles/deepseek-harness-super-agent （没细读）。
  - 仓库 README 自己说明：原型和文章描述的实现**不完全一致**，文章里有些设计只是未来方向，原型里没做。
- 原型是什么：它是 DeepSeek Harness（下文简称 DSH）的插件，充当一个仓库的"主动编码 agent"。事件从 GitHub/飞书进来，主 Session（Router）负责评估和派工，每个 Ticket 起一个独立的 worker Session 和一个独立目录去干活。状态存在 SQLite。

## 0. 概念速览（001）
Source（事件源）→ Trigger（不可变事件，是审计和幂等的单位）→ Thread（事件流，同一时刻最多跑一个 Eval）→ Eval（两层判断：确定性 gate + 模型研判）→ Decision（accept/stop/ignore/unavailable）→ Ticket（一份已授权的工作）+ Mandate（不可变的授权凭据）→ Assignment（Ticket 绑定到某个执行器实例）→ Report（progress/waiting/ready/blocked/failed/dropped）。Subscription 是 Ticket 和 Thread 之间的多对多关系，只用来投递上下文，不带任何授权。

## 1. 可借鉴思路（每条都标了【已实现】或【仅文章讨论】）

### 1.1 Mandate：把"为什么允许"冻结成不可变凭据，恢复执行前重新校验【已实现】
- **解决什么**：策略会变（白名单、规则、模式），长时间运行的任务需要知道当初是凭什么被批准的，也要决定策略变了之后还能不能继续。
- **做法**：accept 时生成 Mandate，记下当时的策略版本、范围、强度（explicit 或 inferred）、准入依据（via：addressed/admission/followup/bot/listed）以及请求者。Mandate 之后不再修改，要扩大范围就得开新 Ticket。只有三种操作能改变它：accept 创建、人工 stop 撤回、重新校验失败时吊销。已结束的 Ticket 要恢复，必须先按"当前策略"对 Mandate 重新校验，按依据类型逐项检查，比如表格准入要求当前格子的强度不低于原来的强度。策略变更**不会**直接取消正在跑的 Ticket，只会向受影响的 Ticket 投递一条 policy-changed 输入，同时作废进行中的 Eval，把它们重新排队。
- **映射到 OS**：可以对应 M1 的 Ed25519 令牌。令牌里写死"签发时的策略版本、依据、范围、强度"，令牌本身不可变；任务从挂起/阻塞恢复时调用 `revalidate(token, current_policy)`。策略变更时不硬杀任务，而是给任务发一条"策略变更通知"，下次恢复时再判定，这样能避免留下做了一半的副作用。
- **不适用之处**：原型对运行中的任务"不撤销、只通知"，因为它的副作用主要是开 PR、发评论，相对温和。OS 里"外部副作用"和"读敏感值"这两类能力应该**立即失效**，不能等到下次恢复才检查。另外 Mandate 只是数据库里的记录，没有签名。
- **依据**：docs/001-domain.md（概念表 Mandate/Policy 行）；docs/002-authorization.md 的 "Policy version"、"Resume and stop"、"Revalidation before resume" 三节；`src/core/model.ts:104-133`（Mandate 和 MandateBasis 的类型）；`src/policy.ts:143-165`（policyEffects：算出每个 Ticket 受了什么影响）、`src/policy.ts:213-240`（revalidate）；`src/core/tickets.ts:113,257`（恢复/attach 时重新校验）。

### 1.2 两层准入：先走确定性 gate，再走模型研判；强度分级，主动工作默认只观察【已实现】
- **解决什么**：输入里绝大多数是噪声；如果让模型逐条判断，既贵也不安全；主动行为还需要有个灰度上线的过程。
- **做法**：第一层 gate 是纯代码，按事件源、作用域、发送者白名单、"是否点名 agent"查一张准入表，给出 explicit / inferred / 不准入。bot 最多只能拿到 inferred，而且不能发 stop。第二层才交给模型，按团队写的规则研判，每条启用的规则恰好判定一次，主动接单至少要匹配一条规则。accept 模式分三档：off / observe（默认，只记录"本来会接"）/ auto。消息、评论、引用的文档都只是判断的输入，**改不了权限**。
- **映射到 OS**：语义界面的事件进入时，先用确定性规则（来源、主体、能力类）分级，再交给 agent。新上线的"主动能力"先跑 observe 模式，只写审计、不执行，人工复核后再切到 auto。这和人工确认状态机可以叠在一起用：observe 相当于"影子模式"。
- **不适用之处**：原型的 gate 只看身份和来源，不看动作的危险等级；动作分级在 OS 里要另外做（对应六类能力）。"explicit 请求在所有模式下都放行"这一点，对外部副作用类能力不能照搬，因为 OS 要求外部副作用每次都人工确认。
- **依据**：docs/002（Two paths / Strength / Admission / Accept mode and rules）；`src/policy.ts:166-212`（gate、machineGate、admitsInferred）；文章 3.1 节 Fast/Slow Path（文章讨论，原型已实现）。

### 1.3 Ticket 阶段是从事实推导出来的，不是手工维护的状态位；"等人"类阶段排在最前【已实现】
- **解决什么**：如果任务状态由执行器自己报，会乱，还会和执行器 Session 的状态耦合；人工面板需要互斥、完备、可检查的分类。
- **做法**：`ticketStage()` 是一个纯函数，按优先级逐条匹配：已结束 → 输入投递失败（person/delivery）→ 有待回答的问题（person/answer）→ 最近一次报告是 ready（person/review）→ blocked/failed 且之后没有人再发过消息 → 没有 Assignment 或 Assignment 还在 pending（queued）→ waiting 或 Assignment 已结束（idle）→ working。Ticket 和 Assignment 是两个独立的状态机：Ticket 是 open↔ended，Assignment 是 pending→running→ended；一个 Ticket 可以有多个 Assignment；只有最终报告能从执行器一侧结束 Ticket。"人要求停止"只能由 Eval 判定，执行器不能自己声称"人让我停"。
- **映射到 OS**：任务控制块里只存事实（报告、待答问题、投递错误、Assignment），阶段由函数推导出来，保证重启后结果一样（原型就是从存储的报告和输入重新算出来的）。人工确认状态机可以借鉴"person/* 优先"的排序，以及"人的新消息自动解除 blocked"。
- **不适用之处**：阶段划分是为代码协作（PR review）定制的。OS 里应该换成能力确认、配额等待这类阶段。
- **依据**：docs/003-execution.md 的 "Ticket stages"；`src/core/stage.ts:1-26`；docs/001 的 Rejected concepts（不让 Ticket 状态跟随 Session、执行器不能声明停止）。

### 1.4 先落盘再执行：持久化 effect 队列、按种类控制顺序和退避、失败三次才告警【已实现】
- **解决什么**：判定之后的外部动作（转发、启动、投递、回复）可能因为崩溃或网络问题丢失或重复。
- **做法**：Engine 是领域聚合的唯一写者，所有方法都是同步的，一个命令一个事务。外部工作在事务内写入 SQLite 的 effect 表，提交后才分发出去。同种类、同 key 的 effect 合并；可以指定按序执行，失败的会挡住同种类后面的项；指数退避（投递 2s 起、最长 1 分钟，回复 5s 起、最长 300s）；同一项连续失败 3 次才在健康面板标为系统问题。出站消息有回执记录（pending/sent/dropped），丢弃时也保留"何时、为何"，保证不会再次入队。
- **映射到 OS**：内核/系统服务里，"决定"和"副作用"之间用 intent log（先写意图、提交后执行）隔开，配合幂等 key 和单写者。人工确认通过后真正发出外部请求，也应该走这条路径，崩溃重启后不会重复发送。
- **不适用之处**：原型依赖 SQLite 事务和 JS 单线程同步；内核里要自己实现日志和崩溃一致性。退避参数是为网络服务设定的。
- **依据**：docs/003 的 "Stored work"、"Channel failure"；docs/007-architecture.md 的 Engine、Persistence 节；`src/app/effects.ts:25-60`；`src/app/store.ts:134-160`（提交后回调、回滚时丢弃）。

### 1.5 恢复与重试：区分瞬时错误和真实失败；重启后带"恢复说明"重派，并做投递去重【已实现】
- **解决什么**：长任务被重启打断后，既不能丢掉输入，也不能重复做已经产生的外部副作用。
- **做法**：只有瞬时错误（408/429/5xx、连接被拒绝/重置/超时）会自动重试，最多 3 次，退避从 15s 开始翻倍；prepare 失败不重试，启动失败也不自动重试。blocked/failed 之后，Ticket 不再接收自动输入（CI 和 bot 消息只记录、不唤醒），但工具保持可用，人可以直接接手。Retry 会结束旧的 Assignment，在同一个 Session、目录、分支下开新的 Assignment。重启时：正在运行的 Assignment 标记为 "host restarted"，从最老的 Ticket 开始，带一份恢复说明重新启动；说明要求执行器在重复任何外部动作之前，先查目录、git 历史和远端状态。系统逐条记录"哪些输入已经进了 Session 历史"：已认领但被打断的算未读，并且会去掉残留副本，保证新 Assignment 只收到一份。Eval 超时会重新排队 2 次，第 3 次记为 unavailable，只在 24 小时内、且仍是最新决定时才打扰人。
- **映射到 OS**：任务恢复协议可以这样设计：瞬时错误由内核自动退避，语义失败交给人；恢复时给 agent 注入一条"先对账、后重做"的系统提示；IPC 消息带"已消费"水位，做 exactly-once 投递。
- **不适用之处**：靠提示词让 agent 自己对账，不是强保证。OS 里外部副作用应该有可查询的幂等记录（见 1.4），不能只靠模型去查 git。
- **依据**：docs/003 的 "Failure and recovery"（Executor blocked/Retry/Restart）；docs/002 的 "When Eval does not finish"；`src/executor/dsh.ts:29-32,548-575,603-660`；`src/core/assignments.ts:79`；`src/core/evals.ts:18,58-62`；`src/core/stage.ts:23-26`（canRetry）。

### 1.6 Executor 隔离：上下文和读取范围强隔离，但进程级隔离明确声明"没有"【部分实现】
- **解决什么**：并发任务之间不能互相污染上下文，也不能越权读取别的任务。
- **做法**：每个 Ticket 一个独立 Session、一个独立目录（通常是 git worktree）。worker 的读取资源由宿主限定，只能读自己的 Ticket、Thread、自己收到的 Trigger 和原始记录；分页里会隐去它读不到的 id（其他 Ticket、Decision）。主 Session 用工具更少的 router 预设。执行变量不包含凭据；blocked commands 只是按命令名拦截。**文档自己承认**：wrapper 和目录只是默认配置，不是安全边界。两个预设都以运营者的 OS 用户、danger-full-access 运行，能通过绝对路径执行程序、能读 home 目录；要隔离只能另开专用 OS 账户。只有面板能改 shell 命令，发给主 Session 的文本改不了。
- **映射到 OS**：可以借鉴"读权限按任务裁剪、引用图里隐去不可达对象的 id"（能力不可见即不可达）。它承认缺失的那部分，正是自研内核该补的：以 Ring 3 任务和能力句柄做真正的边界。
- **不适用之处**：按名字拦命令、靠提示词约束合并行为，在 OS 里都不能算安全机制。第三方分析也指出，主 Session 能改团队技能、策略和 accept 模式，只是靠文字要求它等人确认（这一点我没在源码里逐项核实）。
- **依据**：docs/003 的 "The Executor"、"Execution variables"；docs/006-deployment.md 的 "Permissions and isolation"（约第 13-19 行）；docs/007 的 "Read resources"（约第 550-580 行）；docs/007 的 Open questions（信任边界尚未解决）。

### 1.7 人类注意力预算：把"打扰人"当作稀缺资源来设计【已实现（大部分靠提示词和规则）】
- **解决什么**：多任务的上限是人的注意力（文章说一个人大约同时只能盯 3-5 个会话），而不是模型。
- **做法**：只在被请求或有书面规则时才行动；只在需要人行动、或结果对人有用时才说话：确认一次、做完时、需要决策时、做不下去时。一次只问一个具体问题，而且用结构化字段问（Attention），不让问题埋在长报告里；确认消息不超过 100 词。并发上限之外**为人的消息额外保留 1 个槽位**，新任务不会挤掉人对已有任务的跟进。面板只显示"需要人"的内容，健康状态不占位置；一次性的网络错误不告警；计数必须等于它链接到的列表长度。"等人"这个状态不从自由文本推断，必须来自结构化字段。
- **映射到 OS**：人工确认队列要做准入控制和合并：每任务同时只能有 1 个待确认问题，给人的交互预留调度配额，确认请求用结构化 schema。可观测性方面，默认只暴露需要人处理的条目。
- **不适用之处**：OS 里外部副作用每次都要确认，打扰频率天然更高；需要再加批量确认或按会话授权等机制，原型没有。
- **依据**：docs/000-philosophy.md 全文；docs/005-panel.md 的 Principles；docs/003 的 "Concurrency"；`src/executor/dsh.ts:166-190`（WorkerSlots 和额外槽位）；docs/007 第 534 行附近（确认消息 100 词上限）；文章 1 节、4.2 节。

### 1.8 订阅直通：后续事件按订阅直接路由给 worker，路由不带授权【已实现】
- **解决什么**：如果后续消息都经过全局 Router，Router 会成为瓶颈，token 也会浪费。
- **做法**：Router 只处理新任务冷启动和跨任务协调；Thread 上后续的人类消息由订阅它的 Ticket 的执行器决定处理还是交回（每条只能交回一次）。上下文订阅只送消息，不赋予跟进权，也不唤醒已结束的 Ticket。Ticket 结束时，未读、未处理的消息会退回给 Eval。文章的消融实验：去掉直通后完成时间差不多，但 Router 决策次数多约 30%，cache 读 token 多约 37%。
- **映射到 OS**：事件总线可以做"订阅即路由，路由不等于授权"：消息投递和能力授予分离。孤儿消息在任务结束时回流到调度器。
- **不适用之处**：OS 里的事件量和延迟要求跟这个场景不同；实验是合成负载，文章自己也说结果只有指示意义。
- **依据**：docs/001 的 Subscription 行；docs/002 的 "Subscriptions" 到 "Context-only subscriptions" 几节；docs/003 的 "Follow-ups and context Threads"；`src/core/model.ts:75`（followupOwners）；文章 3.2 节、7.1 节。

### 1.9 审计：事件不可变、每个 Trigger 都记录去向、决策可回溯【已实现】
- **解决什么**：要能回答"这件事为什么被做了、或者为什么没做"。
- **做法**：Trigger 不可变，是审计和幂等的单位，重复 accept 只产生一个 Ticket。每个 Trigger 都有 Disposition（被过滤、被评估、被扇出）。Decision 记录理由、匹配到的规则，以及是否属于 `ignoredFromCard`（看卡片就忽略）。设置和提示词带版本和保存历史。面板可以从 Trigger → Eval → Ticket 追溯一项工作的完整历史，还能"重新评估"。
- **映射到 OS**：审计日志的基本单位是不可变事件；每条事件必须有去向记录（含"被丢弃及原因"）；授权决定引用策略版本的哈希。
- **不适用之处**：没有防篡改（不签名、不做哈希链）；审计放在同一个 SQLite 里，agent 所在的 OS 用户理论上能改。
- **依据**：docs/001（Trigger、Disposition）；docs/002 的 "Routing candidates"；docs/005 的 Tabs、Reading individual records；docs/004-prompts.md 的 Versions 节（只扫了标题）。

### 1.10 暂停处理：一个全局"刹车"，但不丢事件【已实现】
- **做法**：暂停时停止派工、Eval、投递和回复，Eval 的截止时间顺延；事件源继续记录，正在运行的回合不打断。恢复后第一轮重新校验 Mandate。
- **映射到 OS**：可以做成"全局冻结外部副作用"的开关（类似熔断），同时保留采集。
- **依据**：docs/003 的 "Pause processing"。

## 2. 文章里讨论但原型没有实现（或没核实）的设计
| 设计 | 文章节 | 原型情况 | 对 OS 的启发 |
|---|---|---|---|
| 外部 Watchdog Session：定时检查进度和是否偏离目标，可用自然语言追问和修正 | §6 | 原型没有 Watchdog（只有一个每小时运行 `status` 的检查脚本，见 docs/006 "Hourly check"，我没细读） | 主系统之外的独立控制环，除了检查进程活着，还要检查任务"有没有实质进展" |
| 计算、状态、环境三者分离；端侧轻量 daemon；BYOM | §5 | 原型是单机运行，状态存本地 SQLite | 执行环境通过无状态 daemon 接入；也要注意文章自己指出的风险：云端一旦被攻破，所有端都会被接管 |
| "使用凭据但永远看不到凭据" | §8.4 | 只做到执行变量里不放凭据，用 wrapper 选账户；worker 仍然能读 home 目录 | 正好对应"读敏感值"这类能力：用句柄代替明文 |
| 全双工 start()/stop() 异步 ReAct | §4.1 | 原型用"前台命令 5s 后转后台，回合结束后后台工作最多 30 分钟"来近似（docs/003，`src/executor/dsh.ts:32`），文章说 DSH 有这个机制但默认没开 | agent 的系统调用默认异步，完成后以事件形式回送 |
| 推测执行（静默复现修复，确认后才出结果） | §3.4 | 原型能用 rules 加 silence 规则做到类似效果（docs/002），但不是独立机制 | 后台预先做的工作必须停在"可逆/本地"类能力内，外部提交仍要确认 |
| Router 分片、Worker 互相通信、长期记忆、A2A 的上下文和权限边界 | §2.1、§8.1-8.3 | 未实现，属于开放问题 | 多 agent 实例之间的权限边界通信，是 agent-OS 未来的 IPC 问题 |
| Router 批处理吞吐实验、消融实验 | §7 | 实验数据；没有可复现脚本（我没找到） | 仅供参考 |

## 3. 没读的部分和未验证项
- 只扫了标题或部分段落：docs/004-prompts.md、docs/006-deployment.md（只读了权限与隔离一节）、docs/007-architecture.md 的 Modules、Tests、Sources、Settings 小节、docs/008-cookbook.md（只看了目录）、SETUP.md 后半部分（只读到问题清单开头）。
- 源码只通过 grep 和局部阅读看了 stage.ts、policy.ts（143-242 行）、effects.ts（25-60 行）、store.ts（片段）、executor/dsh.ts（grep 命中行）、model.ts（片段）；没读 tickets.ts、evals.ts、assignments.ts、sessions.ts、migration.ts 的完整内容，也没读前端代码和测试。
- 行号以上述提交为准，有些是 grep 定位的，范围可能有几行偏差。
- 没有运行、构建或测试过原型；没有验证文档描述和代码行为是否一致。
- 文章官方原文链接没确认（X 页面返回 404）；读的是转载站的机翻英文版，可能和中文原文有出入。
- "主 Session 能改策略和 accept 模式、只靠文字约束"这条来自第三方站点（neoneye agent-memory-atlas），我没有在源码中核实。

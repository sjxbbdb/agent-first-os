# 第七轮调研：系统原生的 agent 电脑操控（computer-use）v2

> 入库说明（方案规格，2026-11-02）：内核调研出具的 v2 原文，只做称呼与环境措辞的替换（对项目负责人的称呼统一写作「项目负责人」，对调研所在机器的说法统一写作「调研环境」），其余未改。本文是 M2 以后的设计输入，**不改变 M1 范围**。「已决定」三条来自总经理转达的项目负责人决定（2026-11-02），无独立原始记录；末尾三条「仍待项目负责人拍板」都不是定论。正文里「T-M1-2 实现中」「49 号」等描述是调研当时的说法，实现进度以任务清单状态表为准。


调研时间：2026-11-02（北京时间）。只做调研，不含内核代码。这份文件是 M2 以后的设计输入，M1 范围不变。
标注：[Sx] 对应文末来源清单，都是实际打开读过的；[推断] 是我的判断，没有外部来源；「未验证」表示没核实到。[R3]、[R4] 指本系列第三轮、第四轮报告。[X] 指外部参考材料，只用了公开可见的行为描述，用我们自己的话重新表述，没有引用原文。

**v2 修订点（相对 v1）**
1. 并入外部参考材料 [X] 的 8 条设计思路，逐条核对依据强弱（见 2.0）；第二节接口草案重写：增量树格式、元素句柄版本与 expected_tree_version、审批分级与持久化对应的能力位、两阶段 prepare/commit 和 round4 令牌对齐。
2. 新增 0.2：GUI 的决定项目负责人已搁置，M2 按无头结构化界面设计。
3. 更正 syscall 49：已在 M1 规格 v0.15（提交 60f1003）定稿为 `SYS_RESOURCE_REQUEST`，T-M1-2 实现中。v1 正文里「核对不到」「语义未验证」的说法全部删掉，v1 末尾的补注并进 0.1。
4. 新增第六节「批量预批对 round4 令牌格式的影响（初步设计选项）」。
5. 拍板决定、未验证清单、来源清单同步更新。v1 备份在调研环境的临时目录，不进仓库。

## 〇、先核对 M1 现状

### 0.1 syscall 49
- **状态**：M1 规格 v0.15（提交 60f1003，2026-11-02 20:18 北京时间）把 49 号定稿为 `SYS_RESOURCE_REQUEST`（结构化资源申请），对应任务 T-M1-2，**实现中**。实现是否完成以仓库为准 [S20]。
- v1 写「仓库副本里没有 49」，是因为调研环境里那份副本的 `syscall.h` 最大号只到 47 [S0]，而规格里写的实施基线 8fb9198 最大号是 48（`SYS_CLOCK_TICKS`，T-M1-1）[S20]。副本比规格基线还旧，所以没有 49 是正常的。
- 我从规格里读到的内容（提交 60f1003）[S20]：
  - 调用约定：`int 0x80`，`rax=49`，`rdi` 指向用户态请求结构，`rsi` 是长度，必须正好是 32。内核先检查这 32 字节是不是该任务可读的用户内存，再整块拷到内核栈上，只校验这份副本。
  - 请求结构 32 字节：version(u16)=1、size(u16)=32、flags(u32)=0、kind(u32)=1（页）、count(u32) 取 1–4、request_id(u64) 非 0、reserved(u64)=0。
  - 校验顺序固定为 LEN、FAULT、VERSION、SIZE、FLAGS、KIND、COUNT、REQID、RESERVED、QUOTA，多个错误同时存在时只报第一个。
  - 返回码：0 表示批准；-14 为 FAULT；-22 为各类畸形；-95 为不认识的 kind；-122 为超配额；-12 为页池空（NOMEM，M1 用例不触发）。一次申请要么全批要么全拒。
  - 限额：每任务页配额 20，单次上限 4，内核页池 32 页。限额在任务创建时由内核传入，Ring 3 不能自己指定。
  - 资源形态：记账型页配额，任务不能访问这些页，M1 不释放。
  - 50、51 号分别预留给 T-M1-3、T-M1-4，不实现；127 不得占用。
- **对本报告的意义**：49 是「按 kind 记账的配额申请」，现在只有 kind=1（页）。界面动作的配额（每分钟动作数、COMMIT 次数）可以考虑以后加一个新 kind 复用这条路径。这是 [推断]，规格没有提到，要走 T-M1 之后的流程另定。

### 0.2 GUI 方向
项目负责人已把 GUI 的决定搁置。M2 按**无头结构化界面抽象**设计：不画像素，应用直接上报语义树，agent 只通过 IPC 读状态、发动作。v1 拍板决定第 1 条（要不要移植图形栈）因此不再需要决定，第四节的比较留作以后的参考。

### 0.3 其他 M1 组件
- 已核到的能力和令牌 syscall：`SYS_CAP_RESTRICT/TRANSFER/REVOKE = 24–26`，`SYS_POLICY_TOKEN_MINT/CONSUME/REVOKE = 40–42`，`SYS_POLICY_PAUSE/RESUME = 43–44`，`SYS_GROUP_FREEZE = 7` [S0]。
- 能力是指向内核对象的句柄加权限位，内核只认任务、能力、确认令牌、日志四类对象 [R3]。
- 确认令牌：Ed25519，内核只放公钥。160 字节，字段有 key_id、boot_generation、seq、task_id、capability、action_nonce（内核生成）、action_hash[32]、expires_at（按内核单调时钟算）[R4]。M1 规格写明令牌签名算法已定为 Ed25519，但令牌字段属于 T-M1-3/T-M1-4，「仍待写」，round4 的格式只是调研建议 [S20]。

---
## 一、现有方案的缺口

### 现状
- **Linux AT-SPI**：自由软件系统无障碍栈的核心，包含无障碍守护进程、ATK 库以及 AT-SPI 和 ATK 之间的钩子 [S1]。元素可以执行 `do_action` [S2]，有 `get_extents`、`grab_focus` 等组件接口 [S3]；事件有 `object:state-changed`、`object:children-changed`、`object:text-changed`、`object:property-change:accessible-name`、`window:minimize` 等 [S4]。
- **Windows UI Automation**：让应用提供和消费 UI 的程序化信息，能访问桌面上大多数 UI 元素，也专门为自动化测试设计 [S5]。树有 raw、control、content 三种默认视图，按过滤方式区分 [S6]。行为用 control pattern 表达，比如按钮用 Invoke、带滚动条的控件用 Scroll [S7]。事件有属性变化、结构变化（元素出现、隐藏、移除）、全局桌面变化（焦点切换、窗口关闭）[S8]。
- **macOS AX**：`AXUIElementCreateApplication` 按 pid 取应用顶层对象，`AXUIElementCopyAttributeValue` 读属性，`AXUIElementPerformAction` 请求执行动作，`AXUIElementCopyElementAtPosition` 按屏幕坐标取元素，`AXObserverAddNotification` 订阅通知，`AXIsProcessTrusted` 判断当前进程是不是受信任的无障碍客户端 [S9]。
- **输入注入**：
  - uinput 让用户态写 /dev/uinput 来创建虚拟输入设备 [S10]。
  - libei 是面向 Wayland 栈的模拟输入库，分 EI 客户端和 EIS 服务端，通过 UNIX socket 通信；可以经 XDG RemoteDesktop portal（liboeffis）接入 [S11][S12]。libei 的 README 写明，服务端**控制**模拟输入，可以过滤或丢弃，比如焦点在密码框时直接丢掉模拟输入 [S12]。
- **截图加坐标**：
  - Anthropic 的 computer use 工具提供截图和鼠标键盘控制，坐标是截图像素空间 [S13]。
  - OpenAI 的 computer use 指南也是「执行动作、回传截图」的循环 [S14]。

### 痛点（逐项带依据）
| 维度 | 痛点 | 依据 |
|---|---|---|
| 可靠性 | 截图方案对下拉框、滚动条这类元素操作困难，官方建议改用键盘快捷键 | [S13] |
| 可靠性 | 无障碍树质量取决于应用有没有实现；UIA 要求开发者遵循无障碍设计 | [S5]；「很多应用实现不全」是 [推断] |
| 延迟 | 每一步都要截图回传、模型再看图，OpenAI 指南建议「先截图再行动、几步之后再检查屏幕」 | [S14]；具体延迟数字**未找到** |
| 不可审计 | 坐标点击只记录「点了 (512,742)」，不记录点的是哪个元素、当时屏幕上是什么 | [S13] 的坐标模型 + [推断] |
| 提示注入 | 网页或图片里的指令可能覆盖用户指令；Anthropic 用分类器扫描截图，OpenAI 要求「屏幕内容不可信，不能授予权限」 | [S13][S14] |
| 权限粒度 | macOS 是进程级的「受信任客户端」开关，一旦受信任就能读写所有应用（读写范围是 [推断]）；Windows 用 UIAccess 和完整性级别控制能否访问高权限 UI，也只是按进程分级 | [S9][S15] |
| 权限粒度 | uinput 创建的是系统级设备，注入的输入和真实输入在下游难以区分 | [S10] + [推断] |
| 隔离 | 两家都要求在专用 VM/容器、最小权限下运行，Anthropic 参考实现跑在 Docker 里 | [S13][S14] |

### 改法
系统原生方案的三个核心：**语义优先**（元素 ID + 动作，而不是坐标）、**每个动作都绑定元素和状态版本**、**权限按元素和动作类型给**。借鉴：UIA 的 control pattern、AT-SPI 的 action 接口、libei 的「服务端掌控、可过滤」[S2][S7][S12]。

### 风险
- 应用不上报语义树，等于这个应用对 agent 不可见。自研系统里的应用都是新写的，可以强制要求，但以后移植的程序做不到 [推断]。
- 语义树可以说谎（元素欺骗），见第五块。

### 怎么验证
第五块的用例 U1–U6。

---

## 二、agent 专用接口草案 v2（草案，不是最终设计）

### 2.0 外部参考材料 8 条思路的核对
材料是对一个闭源电脑操控产品的逆向分析，里面有字符串、实测输出和协议结构。下表只写我读到的行为层面依据，不写对方的任何内部名称 [X]。

| # | 思路 | 材料里的依据 | 强弱 |
|---|---|---|---|
| ① | 读状态和动作分离，动作后必须重新取状态 | 取状态是单独的工具，要求每一轮交互前先调一次；动作完成后的返回提示是「去重新取状态」 | **强** |
| ② | 树压缩成缩进文本加编号，第二次只发增量 | 实测输出里，层级用行首缩进表示，每行是「编号 角色 标签」；第二次起默认是相对上一棵树的差异，分成「改」「增」两种标记，删除按编号区间汇总，末尾单独给出当前焦点元素；也有「相对初始树的累计差异」和「无变化」两种形态；首次访问或关闭差异时发全量 | **强**（有实测样本） |
| ③ | 元素句柄带版本，动作前校验，过期就拒 | 元素靠编号（树里的行号）寻址，**编号本身不带版本**；动作前会重新取元素并校验元素还在、类型没变；另一条第三方驱动路径有「元素过期」的错误 | **中**：「动作前校验、过期拒绝」有依据，「句柄自带版本号」是我们的设计，材料里没有，标 [推断] |
| ④ | 动作种类少，优先无障碍式设值和次级动作，坐标点击只作后备 | 动作集合确实小（点击、次级动作、设值、选文本、滚动、拖拽、按键、输入文本，大约九种）；点击可以按编号，也可以按截图坐标；拖拽只支持坐标 | **弱**：「种类少」「有设值和次级动作」有依据；「坐标只作后备、优先语义」在材料里**没找到明确说法**，未验证 |
| ⑤ | 按应用审批分级与持久化（仅此次、本会话、永久、可撤销） | 第一次用某个应用要先审批，可以选「本会话」或「永久」，永久的会记到本地；设置页能撤销，撤销后下次重新询问；组织策略禁止和安全原因禁止的直接拒绝、不弹审批；另一个集成里持久化有四档（一次、本轮、本会话、永久） | **强** |
| ⑥ | 确认四档：必须交还用户、动作时必须确认、可预批准、不需要 | 确认策略原文就是这四档，并有逐类示例（改密码和金融交易必须交还；永久删除、接受法律协议要动作时确认；删除可恢复数据、上传文件等可预批；只读、下载不需要）；还要求不要过早确认、动作或目标或数据没变时不要重复确认 | **强** |
| ⑦ | 发送类动作两阶段 prepare 再 commit | 消息发送分成「准备」和「提交」两步：准备时给出收件人、正文、附件，返回一份带计划 ID 的待发送内容；提交时只带计划 ID；有防重复发送的错误提示 | **强**，但只在一个消息集成里看到，通用界面动作里没看到 |
| ⑧ | 动作事件流用 JSONL，可回放，有时长上限和白名单 | 有「记录用户动作」功能，单次最长 30 分钟；记录写成 JSONL 事件流；有按应用或域名的白名单、黑名单；回放是让模型读事件流后**起草回放计划**或技能文档 | **中**：JSONL、时长上限、白名单有依据；「可回放」在材料里是模型读日志起草计划，**不是确定性重放**，我们要的确定性回放是 [推断] |

总结：①②⑤⑥⑦依据强；③⑧部分有依据；④里「坐标只作后备」没依据。

### 2.1 现状和痛点
M1 没有界面相关对象 [S0]。现有系统的结构化状态、输入注入、变化事件分散在三套机制里（无障碍树、输入注入、截图）[S1][S11][S13] + [推断]。

### 2.2 总体结构
用户态界面服务 uisvc，加应用侧上报。全走已有的 `SYS_IPC_*`，内核只做能力检查、令牌消费、审计追加 [S0][R3]。
**读和写分开（思路①）**：`UI_OBSERVE` 只读，`UI_ACT` 只写。`UI_ACT` 的返回值**不带新树**，只带结果码和新版本号；agent 要看结果必须再调一次 `UI_OBSERVE` [X] + [推断]。这样所有状态都走同一条可审计的路径。

### 2.3 增量树格式（思路②）
文本格式，每行一个元素，UTF-8，行首 Tab 个数表示深度：
```
@tree win=3 ver=17 base=- mode=full
<id>.<gen> <role> "<name>" [states] {actions} !<effect>
0.1 window "表单"
	1.1 text_input "收件人" [focused] {write_value,focus} !reversible
	2.1 text_input "正文" {write_value,focus} !reversible
	3.1 button "保存" {invoke} !commit
	4.1 button "发送" {invoke} !external
@focus 1.1
```
第二次起默认发增量，相对 agent 上一次拿到的版本：
```
@tree win=3 ver=18 base=17 mode=diff
~ 1.1 text_input "收件人" value="ops" {write_value,focus} !reversible
+ 5.1 label "已保存草稿"
- 7-9
- 12
@focus 2.1
```
- `~` 改、`+` 增、`-` 删除按编号区间汇总，`@focus` 单独一行，这是借鉴 [X] 的思路，符号是我们自己选的 [推断]。
- `base` 必须等于 agent 上一次拿到的 ver，不等就发全量（mode=full）。agent 也可以主动要全量 [推断]。
- name 和 value 一律加引号，引号和换行转义，避免界面文本伪造出一行新元素（见第五节的注入风险）[推断]。
- 视图过滤沿用 UIA 的 raw/control 思路 [S6]：默认发 control 视图，去掉纯装饰元素。

### 2.4 句柄版本和 expected_tree_version 的关系（思路③）
两个层次，各管一件事：
| 字段 | 含义 | 什么时候变 | 管什么 |
|---|---|---|---|
| 元素句柄 `id.gen` | id 是窗口内编号，gen 是这个编号的代数 | 元素被删、编号被另一个元素复用时 gen+1；元素属性变化时**不变** | 防「编号复用」：旧句柄指不到新元素 |
| `tree_version` | 整个窗口的单调版本号 | 任何结构变化或非 volatile 属性变化都 +1 | 防 TOCTOU：agent 看到的是不是执行时的状态 |

`UI_ACT` 带两个前提：`elem = id.gen` 和 `expected_tree_version`。uisvc 的校验顺序（草案）：
1. 句柄 gen 不匹配 → `STALE_HANDLE`（元素已经不是原来那个了）；
2. 元素不支持这个动作 → `BAD_ACTION`；
3. `expected_tree_version` 不等于当前版本：
   - 默认 → `STALE_TREE`，要求重新 OBSERVE；
   - agent 显式声明「只要求元素没变」（`match=element`），并且该元素自 expected 版本以来没变过（uisvc 为每个元素记最后修改版本）→ 放行。这个放宽只对 L1 可逆动作开放，L2/L3 一律要求整树版本相等 [推断]。

借鉴：材料里动作前重新取元素并校验元素还在、类型没变 [X]；这里把它做成显式字段，好审计 [推断]。

### 2.5 动作种类（思路④，依据弱）
| 动作 | 参数 | 对应现有系统 |
|---|---|---|
| `INVOKE` | 无 | UIA Invoke pattern [S7]，AX PerformAction [S9]，AT-SPI do_action [S2] |
| `ALT_ACTION` | 动作名（元素自己声明的次级动作列表里选） | AT-SPI 一个元素可以有多个 action [S2] |
| `WRITE_VALUE` | 文本或数值 | 材料里的设值 [X]；UIA ValuePattern（我只读到 control pattern 总览 [S7]，ValuePattern 页面没读，未验证） |
| `FOCUS` | 无 | AT-SPI grab_focus [S3] |
| `SCROLL` | 方向、页数 | UIA Scroll pattern [S7] |
| `TEXT_RANGE` | 起止，或者前后文 | 材料 [X] |
| `RAW_KEY` / `RAW_POINTER` | 键、坐标 | 后备，默认不授权（见 3.1 的能力位） |
无头系统里根本没有像素坐标，所以 `RAW_POINTER` 在 M2 不实现。「语义优先、坐标后备」是我们自己的选择，不是从材料里得来的 [推断]。

### 2.6 审批分级与持久化（思路⑤⑥）落到能力位
分两层：**应用级授权**决定 agent 能不能碰某个应用的窗口；**动作级确认**决定某一次动作要不要令牌。

**应用级**：uisvc 给 agent 任务发窗口能力（IPC 端点能力），持久化方式用能力本身的属性来表达 [推断]：
| 持久化 | 落到哪里 | 撤销 |
|---|---|---|
| 仅此次 | 不发长期能力；单次动作附带令牌（见 2.7） | 令牌用完即作废 |
| 本任务（对应材料里的「本会话」） | 窗口能力，任务结束时随任务销毁 | `SYS_CAP_REVOKE` [S0] |
| 永久 | policyd 持久化一条「应用 X 对 agent Y 允许」的策略，每次任务启动时 uisvc 照着发能力 | 删策略，并对存活的能力 `SYS_CAP_REVOKE` |
| 禁止 | 策略里标禁止，不弹审批，直接拒（材料里组织策略和安全原因两类都是直接拒 [X]） | — |
「永久」不进内核：内核只认能力，持久化是 policyd 的策略文件 [R3] + [推断]。

**动作级**：沿用 v1 的 L0–L3，和 [X] 的四档对应如下：
| 我们的级别 | 对应 [X] 四档 | 能力位 | 令牌 |
|---|---|---|---|
| L0 只读 | 不需要 | `UI_READ` | 不要 |
| L1 可逆输入 | 不需要 | `UI_ACT_REVERSIBLE` | 不要 |
| L2 本地提交 | 可预批准，或动作时确认 | `UI_ACT_COMMIT` | 要；可以按第六节的方案批量预批 |
| L3 外部副作用 | 动作时必须确认 | `UI_ACT_EXTERNAL` | 每次都要，没有预批 |
| L4 必须交还 | 必须交还用户 | 不存在这个能力位 | agent 无法执行，uisvc 直接返回 `NEED_OWNER` |
另有 `UI_READ_SENSITIVE`（读密码等敏感元素的值）和 `UI_INJECT_RAW`（原始输入），默认都不发。

说明：
- L4 是 v2 新增的。[X] 里改密码、金融交易这类要求交还给人，而本系统没有本地人类界面，所以「交还」的含义是 agent 无法完成、只能报告给项目负责人，由项目负责人在系统外处理 [推断]。
- 「往敏感字段输入也算传输」这条原则同时出现在 [X] 和 OpenAI 的公开文档里 [S14]，所以 SENSITIVE 元素上的 WRITE_VALUE 按 L3 处理 [推断]。

### 2.7 两阶段 prepare/commit（思路⑦）和 round4 令牌对齐
适用于 L2、L3 动作。

**prepare（agent → uisvc，不产生任何副作用）**：
```
UI_PREPARE { win, elem=id.gen, expected_tree_version, action, arg }
→ { prep_id, action_hash, summary, tree_version, expires_hint }
```
- uisvc 做 2.4 的全部校验，然后**冻结窗口**：请求应用进入「只读」状态，或者用 `SYS_GROUP_FREEZE` 冻结应用任务 [S0] + [推断]。
- 内核为这次 prepare 生成 `action_nonce`，并记下单调时间 t0（round4 3.4 的设计）[R4]。
- 计算 `action_hash = SHA-256("AFOS-UI-v2" || app_task_id || win || id || gen || role || name || action || SHA-256(arg) || tree_version || prep_id)`。
- `summary` 是 uisvc **按策略生成**的人类可读描述（比如「在『表单』应用里点击『发送』，收件人 ops」），不直接用应用自报的 name（防元素欺骗）[推断]。

**确认**：policyd 把 summary、action_hash、action_nonce 推给项目负责人的确认端；项目负责人批准后，确认端用私钥签 round4 的 160 字节令牌 [R4]。令牌绑定：task_id、capability（窗口能力）、action_nonce、action_hash、expires_at、boot_generation、seq。

**commit（agent → uisvc → 内核）**：
```
UI_COMMIT { prep_id, token }
```
- uisvc 按 prep_id 找到冻结时的记录，重新计算 action_hash。
- 内核 `SYS_POLICY_TOKEN_CONSUME` 依次校验：签名、boot_generation、seq 没用过、task_id 和 capability 与调用者一致、action_nonce 是内核自己发出且仍待确认的那一个、action_hash 一致、单调时钟没超过 t0 加有效期 [R4] + [推断]。全部通过后令牌作废，uisvc 才把动作交给应用。
- commit 的参数里**没有动作内容**，只有 prep_id（材料里提交也只带计划 ID [X]）。这样 agent 没法在 prepare 和 commit 之间偷换参数。

**prepare 和 commit 之间界面变了怎么办**：
1. 默认不会变，因为 prepare 时窗口已冻结。
2. 如果应用不支持冻结，或者变化来自外部（网络推送），uisvc 在 commit 时发现 tree_version 已经不等于 prepare 时的值 → 返回 `STALE_TREE`，**不调用内核消费令牌**。令牌没被消费，但它绑定的 action_hash 里包含旧 tree_version，以后也不可能再匹配上；内核把这个 action_nonce 标为作废 [推断]。
3. 冻结有时长上限（比如等于令牌有效期），超时自动解冻，plan 作废 [推断]。
4. agent 要重新 OBSERVE、重新 prepare，项目负责人要重新确认。这就是「宁可多问一次」，和 round4 重启后作废的原则一致 [R4]。

### 2.8 动作事件流与回放（思路⑧）
- 审计流是 JSONL，每个 `UI_OBSERVE`、`UI_ACT`、`UI_PREPARE`、`UI_COMMIT` 各一行，字段：任务、窗口、句柄、动作、参数哈希、版本前后、差异哈希、令牌 seq、结果码 [推断]。
- 记录有时长和条数上限；只对白名单应用开启（借鉴 [X]）。
- **确定性回放**（我们自己的设计，材料里没有）：从同一份全量快照出发，按顺序重发 L0/L1 动作，逐步比对版本前后和差异哈希。L2/L3 动作在回放里**只比对、不执行**，因为不可逆动作禁止自动重放 [R4] + [推断]。

### 2.9 风险
- 冻结窗口会卡住应用，项目负责人确认慢的时候，应用看起来像死机。缓解：冻结只针对目标窗口的输入，不冻结渲染和网络（无头系统里只是不接受输入）[推断]。
- 文本差异格式依赖转义正确，转义出错就是注入点，要有专门的模糊测试 [推断]。
- 元素最后修改版本要记在 uisvc 里，内存随元素数增长 [推断]。

### 2.10 怎么验证
用第五节的 U1–U7。

---
## 三、与 M1 组件衔接（v2 更新）

### 现状
能力有限制、转移、撤销三个 syscall [S0]；49 号 `SYS_RESOURCE_REQUEST` 定稿、实现中，目前只有页配额一种 kind [S20]；令牌格式见 [R4]，规格里令牌字段尚待 T-M1-3/T-M1-4 定 [S20]。

### 痛点
现有能力按内核对象授权，没有「窗口 / 元素 / 动作类型」这一层 [S0] + [推断]。

### 改法
1. **能力位**：见 2.6 的表（UI_READ、UI_ACT_REVERSIBLE、UI_ACT_COMMIT、UI_ACT_EXTERNAL、UI_READ_SENSITIVE、UI_INJECT_RAW）。能力可以用 `SYS_CAP_RESTRICT` 去掉权限位后交给子任务 [S0]。元素级范围放在 uisvc 的策略里，不进内核 [推断]。
2. **配额**：每任务的每分钟动作数和 COMMIT 次数。可以考虑以后在 49 号路径上加一个新 kind（比如「界面提交次数」），复用它的固定校验顺序、全批全拒、限额由内核在任务创建时给定这几条规则 [S20] + [推断]。规格里目前只有 kind=1，新增 kind 要按规格流程另定，不能由实现者自定 [S20]。
3. **确认**：L2/L3 走 2.7 的 prepare/commit；L2 的批量预批见第六节。effect_class 以策略为准，应用自报只能往高处调（防元素欺骗）[推断]。
4. **审计**：见 2.8。参数只记哈希；SENSITIVE 元素的值任何时候都不记明文 [推断]。

### 风险
- uisvc 是高权限服务，被攻破就能伪造差异和审计 [推断]。
- 把界面配额塞进 49 号，会让它从「页配额」变成通用配额接口，规格面变大 [推断]。

### 怎么验证
U3、U4、U7。

---

## 四、在自研内核上的现实路径

### 现状
没有图形栈，没有 GPU 驱动，测试用 `-device VGA`，输出走串口 [S0]。

### 选项比较
| 方案 | 读到的事实 | 对本项目的取舍 | 粗略工作量 |
|---|---|---|---|
| A. 无头语义界面（不画像素，应用直接上报语义树） | 现有系统里语义树都是在像素界面之外另加的一层（AT-SPI 通过 ATK 钩子 [S1]），没找到「只有语义没有像素」的现成系统，**未找到** | 最贴合「只给 agent 用」；不依赖 GPU；审计和确认都是结构化数据。缺点是项目负责人想看界面时什么都看不到，要另做一个调试渲染器 | uisvc + 协议 + 一个示例应用 + 测试：4–6 人周 [推断] |
| B. 移植 wlroots 做 Wayland 合成器 | wlroots 是构建 Wayland 合成器的可组合模块，README 自称「约 6 万行你本来就要写的代码」，后端有 KMS/DRM、libinput、Wayland、X11、headless [S16] | 依赖 libc、DRM、libinput、Wayland 协议库，这些自研内核都没有。headless 后端有用，但前提是先有一整套类 POSIX 环境 [推断] | 光 libc 和 POSIX 兼容层就是几十人周级别 [推断]，不适合 M2 |
| C. 参考 Redox Orbital | Orbital 是 Redox 的默认桌面环境，包含显示服务器、窗口管理器、合成器等；他们选它而不选 Wayland/X11，是为了避开 EGL/GLX 的依赖，并且对还不成熟的 Redox API 更简单；支持 Linux DRM API；vesad 直接写 UEFI/BIOS 给的帧缓冲 [S17] | **最值得借鉴的是思路**：自研系统先做一个简单的显示服务，再用固件帧缓冲出图。代码是 Rust，不能直接搬 | 只做「帧缓冲 + 调试渲染」：2–3 人周 [推断] |
| D. 参考 Fuchsia Scenic/Flatland | Scenic 是 Fuchsia 的系统合成器，是一个系统服务，负责全局合成；Flatland 是它的合成 API，FIDL 在 fuchsia.ui.composition [S18][S19] | 借鉴「合成器是系统服务、客户端通过 IPC 提交」的结构；整体太大，不适合移植 | 不建议移植 [推断] |

### 推荐路线
- **M2-a（最小可验收里程碑）：无头语义界面。** uisvc 加一个示例应用（比如一个「表单」应用：两个输入框、一个「保存」（COMMIT）按钮、一个「发送」（EXTERNAL）按钮，发送 = 写 virtio-block 的指定扇区，模拟外部副作用）。agent 任务只能通过 IPC 读树和执行动作。验收用例见第五块 U1–U6，全部用串口输出判定。
- **M2-b**：调试渲染器，把语义树画到 UEFI 帧缓冲，只给人看，agent 不读像素（Orbital/vesad 思路）[S17] + [推断]。
- **M3**：原始输入注入（UI_INJECT_RAW），采用 libei「服务端可过滤」的模型，SENSITIVE 元素有焦点时丢弃 [S12]。
- **以后**：真要跑别人写的 GUI 程序时，放到 microVM 里跑 Linux + Wayland，再用 AT-SPI 把树桥接成本系统的格式（第二轮的结论：computer-use 是兼容场景）[推断]。
- 注：GUI 的决定已搁置（见 0.2），M2 只做 M2-a。M2-b 以后是否做，等项目负责人重新拿出来再说。

### 风险
无头方案会把系统锁死在「必须自己写应用」上。考虑到系统只给 agent 用，这个代价可以接受，项目负责人已决定先按这个方向设计 M2（见 0.2）。

### 怎么验证
M2-a 的用例全部通过，并且对照组成立。

---
## 五、风险与验证（只在 QEMU 里）

### 现状
仓库已有 QEMU 串口 marker 测试的惯例 [S0]；[R4] 已给出令牌的 T0–T3 用例。

### 痛点（注入攻击）
- **界面文本里的提示注入**：两家官方文档都明确承认模型可能听从网页或图片里的指令 [S13][S14]。语义树里的 name/value 同样是不可信文本 [推断]。
- **元素欺骗**：恶意应用把「发送」按钮命名为「取消」，或者把 effect_class 报成 READ_ONLY [推断]。
- **TOCTOU**：agent 看到的是版本 v 的树，执行时界面已经变了（比如弹窗把按钮顶到了原来的位置）。坐标方案对此没有防护 [S13] + [推断]。

### 改法
- 不可信文本：uisvc 给每个 name/value 打 `untrusted` 标记，策略层规定「界面文本不能授予权限」（OpenAI 指南的原则 [S14]）。真正的防线是能力和令牌，不是让模型分辨 [推断]。
- 元素欺骗：effect_class 由策略覆盖（2.6、第三节）；确认端展示的是 uisvc 按策略生成的描述，而不是应用自己报的 name [推断]。
- TOCTOU：`expected_tree_version` 加上把 tree_version 绑进 action_hash；L3 动作执行前冻结窗口 [推断]。

### 验收用例（每条都有对照组）
- 树文本注入：name/value 加引号并转义，防止伪造出新行（2.3）[推断]。
| 编号 | 对照组（先证明能成功） | 实验组（受限后被拒） | 串口判定 |
|---|---|---|---|
| U1 读权限 | 有 UI_READ 能力的任务读树成功 | `SYS_CAP_RESTRICT` 去掉 UI_READ 后读树 → DENIED | `UI case=U1 ctrl=OK exp=DENIED` |
| U2 可逆输入 | 有 UI_ACT_REVERSIBLE 能力时在输入框打字成功，diff 显示 value 变化 | 去掉该权限位后 → DENIED，diff 为空 | 同上格式 |
| U3 本地提交需令牌 | 带有效令牌点「保存」→ OK，扇区内容变化 | 不带令牌 → NEED_CONFIRM；带错误签名的令牌 → BAD_SIG（复用 [R4] T1） | 同上，并且检查扇区未改 |
| U4 外部副作用 | 带有效令牌点「发送」→ OK，写一次 | 同一令牌重放 → REPLAY；令牌是给「保存」签的，拿来点「发送」→ HASH_MISMATCH | `UI effect count=1` |
| U5 TOCTOU | 版本 v 下执行成功 | 测试应用在 agent 取树后自己改结构（版本 +1），agent 按旧版本执行 → STALE，令牌也因 hash 不匹配作废 | 同上 |
| U6 元素欺骗 | 策略未覆盖时，应用把「发送」报成 READ_ONLY，点击成功（证明攻击确实成立） | 策略覆盖为 EXTERNAL 后，同样的点击 → NEED_CONFIRM | 同上 |

要求：每个 case 的对照组和实验组输出各占一行，脚本要求两行都出现、结果都符合期望，单独一行不算通过 [推断]。
| U7 prepare/commit 期间界面变化 | prepare 后不改界面，commit 带有效令牌 → OK，effect count=1 | prepare 后测试应用自己改结构（绕过冻结的测试开关），commit → STALE_TREE，内核计数里该令牌**未被消费**，effect count=0；随后重新 prepare（拿到新的 action_nonce），再用旧令牌 commit → 被拒，原因是 nonce 不匹配 | 同上，并且检查内核 token consume 计数不变 |

### 风险
U6 的对照组和 U7 的「绕过冻结」都是测试开关，测试构建以外的地方不能留 [推断]。

---
## 六、批量预批对 round4 令牌格式的影响（初步设计选项，不定稿）

**适用范围**：只用于 L2 本地提交。L3 外部副作用不管选哪个方案，都**每次一个令牌、每次人工确认**，这条不变 [R4]。

round4 令牌的关键语义 [R4]：160 字节；一个 key_id 一个单调 seq；boot_generation 让重启前的令牌作废；action_nonce 由内核生成，只对内核记得的那一次待确认有效；action_hash 绑定具体动作内容；一次性。

### 方案 a：令牌带范围掩码
令牌里把 action_hash 换成「动作类别描述」：应用、窗口、动作种类集合（比如 {保存}）、元素 role 集合，再加次数上限 N 和更短的过期时间。内核为这个令牌维护一个剩余次数计数器。
- **重放**：seq 和 boot_generation 不变；令牌变成「N 次」，内核每用一次减一，到 0 作废。
- **撤销**：内核令牌表里直接标记作废（现有 `SYS_POLICY_TOKEN_REVOKE` [S0]）。
- **对 action_hash 和一次性的破坏**：大。action_hash 不再绑定具体元素和 tree_version，TOCTOU 防护退回到只靠 uisvc 的版本检查；一次性变成 N 次。
- **复杂度**：令牌格式要加 version=2 和新字段，内核要有计数器表。约 1–2 人周 [推断]。
- **与原则的关系**：只要类别里不允许出现 external 元素，就不冲突。这一点要靠内核校验（类别里的 effect 上限 ≤ commit），不能只靠 policyd [推断]。

### 方案 b：签名一份预批清单（Merkle 根），每次出示证明
项目负责人批准一份清单，清单里每一项是一个具体的 action_hash 模板（应用、窗口、元素句柄、动作、参数哈希，**不含 tree_version**）。确认端只签 Merkle 根加次数上限和过期时间。每次执行时，agent 出示「这一项在清单里」的证明路径，内核验证路径后，再按单项校验。
- **重放**：每一项只能用一次，内核记录已用的叶子编号（位图）；根令牌本身仍带 seq 和 boot_generation。
- **撤销**：作废整份清单（根令牌）；不支持撤销单项，除非再加一层黑名单 [推断]。
- **对 action_hash 和一次性的破坏**：中等。每一项仍然是具体动作，一次性保住了；但因为签名时还不知道执行时的 tree_version，tree_version 只能不绑，TOCTOU 依赖 uisvc。
- **复杂度**：内核要加 Merkle 路径验证（SHA-256，可以用 round4 已选的库里的哈希，Monocypher 没有 SHA-256 这一点 round4 已写明 [R4]），清单格式要定。约 2–3 人周 [推断]。
- **与原则的关系**：不冲突。清单里只允许 commit 类，内核校验每项的 effect 上限 [推断]。

### 方案 c：一动作一令牌不变，policyd 在已签预批策略下代为申请
项目负责人签一份预批策略（比如「本任务内对『表单』应用的『保存』，最多 10 次，1 小时内有效」），签名在 policyd 手里。每次 prepare 时，policyd 判断落在策略内，就自己签一个普通的 round4 令牌。内核验证两样：策略签名（项目负责人的公钥），以及单次令牌签名（policyd 的公钥），并检查单次令牌引用的策略 ID 和次数。
- **重放**：单次令牌完全沿用 round4（seq、nonce、action_hash、tree_version 都在）；策略本身的次数由内核计数。
- **撤销**：作废策略 ID，后续所有单次令牌都过不了。
- **对 action_hash 和一次性的破坏**：最小。每个动作仍是一次性、绑定具体元素和 tree_version 的令牌。
- **复杂度**：内核要信任两把公钥并做两级验证，policyd 要持有一把签名私钥。约 2 人周 [推断]。
- **与原则的关系**：policyd 持有能签单次令牌的私钥，被攻破就能在策略范围内伪造。round4 的倾向是 policyd 不持有私钥 [R4]，这里等于在 L2 范围内放宽了这个倾向。L3 绝对不能走这条路。

### 取舍小结
| | a 范围掩码 | b Merkle 清单 | c policyd 代签 |
|---|---|---|---|
| 一次性 | 变 N 次 | 保住 | 保住 |
| 绑定 tree_version | 否 | 否 | 是 |
| 撤销粒度 | 整个令牌 | 整份清单 | 整个策略 |
| 新增信任 | 无 | 无 | policyd 私钥 |
| 工作量 [推断] | 1–2 人周 | 2–3 人周 | 约 2 人周 |
我的初步倾向是 c，因为它对令牌语义改动最小、TOCTOU 防护最完整。代价是多了一把 policyd 私钥，需要项目负责人拍板（见末尾）。都不是定稿。

---

## 已决定（项目负责人已拍板，v1 的三条拍板项）

1. M2 走无头语义界面，不移植图形栈。
2. 原始键鼠注入默认不给，留到 M3。
3. 元素是否有副作用以策略为准，应用自报只能往高处调。

## 仍待项目负责人拍板的决定（v2 新增，最多 3 条）

1. **批量预批走哪个方案（第六节）。** 倾向 c。理由：每个动作仍是绑定元素和界面版本的一次性令牌，round4 的语义基本不动。代价是 policyd 持有一把只能签 L2 的私钥，所以内核必须强制「policyd 签的令牌只能用于 commit 类元素」。
2. **L2/L3 的 prepare 是否强制冻结目标窗口。** 倾向是强制。理由：不冻结的话，项目负责人确认期间界面变化会让大量确认作废（2.7）；代价是应用要支持「只读状态」，或者被整组冻结。
3. **界面动作配额是否复用 49 号路径（加新 kind）。** 倾向是复用，但放到 T-M1 之后的规格流程里再定。理由：49 的校验顺序、全批全拒、限额由内核给定这几条规则已经定稿 [S20]，复用比新开一个号省事；代价是 49 的规格面会变大。

（v1 的第 1 条「M2 不移植图形栈」项目负责人已搁置 GUI 决定、按无头设计，不再列；v1 第 2、3 条已并入 2.5、2.6 的设计，作为默认做法。）

## 卡点
- 外部参考材料是逆向分析，内容只能按行为层面吸收；思路③的「句柄自带版本」和思路④的「坐标只作后备」在材料里没找到依据，见 2.0。
- Microsoft Learn 页面正文有些抓不全，改读了 GitHub 上的 MicrosoftDocs 原文；Apple 文档改读 JSON 数据接口，只有函数摘要（沿用 v1）。
- Fuchsia Flatland 概念页 404（沿用 v1）。
- 调研环境里的仓库副本比 M1 规格的实施基线还旧，49 号的实现进度没法在副本里看到。

## 未验证清单（v2）
- 49 号实现进度（规格写「证据状态仍为计划」，没有用例在 QEMU 里跑过 [S20]）。
- 49 新增 kind 用于界面配额的可行性（规格没提）。
- [X] 思路④「坐标只作后备」。
- [X] 思路③「句柄自带版本」（我们的设计）。
- [X] 思路⑧「确定性回放」（材料里只有模型起草回放计划）。
- UIA ValuePattern 的具体语义（页面没读）。
- 冻结窗口的具体机制（应用只读状态还是整组冻结）。
- 第六节三个方案的工作量。
- macOS 受信任客户端的访问范围、截图方案的具体延迟、有没有「只有语义树」的现成系统、Wayland 输入注入限制细节（沿用 v1）。

## 来源清单
- [S0] 调研环境里的 agent-first-os 仓库副本：`product/kernel/include/syscall.h` 的 `enum AgentOsSyscall`，编号 0–47（含 7 号 GROUP_FREEZE、24–26 号能力、40–44 号令牌和策略暂停）；副本比 M1 规格的实施基线旧。
- [S1] https://gitlab.gnome.org/GNOME/at-spi2-core/-/raw/main/README.md ：AT-SPI 是自由软件系统无障碍栈的核心，包含基础守护进程、ATK 库以及 AT-SPI 和 ATK 之间的内部钩子。
- [S2] https://docs.gtk.org/atspi2/iface.Action.html ：Atspi.Action 接口，含 do_action。
- [S3] https://docs.gtk.org/atspi2/iface.Component.html ：Atspi.Component，含 contains、get_extents、grab_focus。
- [S4] https://docs.gtk.org/atspi2/method.EventListener.register.html ：事件类型列表，含 object:property-change:accessible-name/description/parent/value、object:state-changed、object:children-changed、object:text-changed、window:minimize/maximize/restore。
- [S5] https://learn.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32 ：UI Automation 是无障碍框架，提供对桌面上大多数 UI 元素的程序化访问，也专门为自动化测试设计。
- [S6] https://raw.githubusercontent.com/MicrosoftDocs/win32/docs/desktop-src/WinAuto/uiauto-treeoverview.md ：三种默认视图 raw、control、content，按过滤方式定义。
- [S7] https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-controlpatternsoverview ：control pattern 暴露控件的某一方面功能，按钮用 Invoke，带滚动条的控件用 Scroll。
- [S8] https://raw.githubusercontent.com/MicrosoftDocs/win32/docs/desktop-src/WinAuto/uiauto-eventsoverview.md ：事件分为属性变化、结构变化（元素出现、隐藏、移除）、全局桌面变化（焦点切换、窗口关闭）。
- [S9] https://developer.apple.com/tutorials/data/documentation/applicationservices/axuielement_h.json （对应页面 https://developer.apple.com/documentation/applicationservices/axuielement_h ）：AXUIElementCreateApplication、AXUIElementCopyAttributeValue(s)、AXUIElementPerformAction、AXUIElementCopyElementAtPosition、AXObserverAddNotification、AXIsProcessTrusted(WithOptions) 的摘要。
- [S10] https://docs.kernel.org/input/uinput.html （以及 https://docs.kernel.org/_sources/input/uinput.rst.txt ）：uinput 让用户态写 /dev/uinput 来模拟输入设备；头文件定义了创建、配置、销毁虚拟设备的 ioctl；推荐用 libevdev 封装。
- [S11] https://libinput.pages.freedesktop.org/libei/ ：libei 是模拟输入库，主要面向 Wayland 栈，客户端和服务端（EIS，一般是合成器）经 UNIX socket 连接；可以经 XDG RemoteDesktop portal（liboeffis）接入。
- [S12] https://gitlab.freedesktop.org/libinput/libei/-/raw/main/README.md ：libei/libeis/liboeffis 三个组件；「服务端控制模拟输入，可以过滤或丢弃，例如焦点窗口是密码框时直接丢弃」；有 sender 和 receiver 两种上下文。
- [S13] https://docs.anthropic.com/en/docs/agents-and-tools/tool-use/computer-use-tool ：截图和鼠标键盘控制，坐标在截图像素空间；下拉框和滚动条难操作，建议用快捷键；模型可能听从网页或图片里的指令，有分类器扫描截图；建议请人确认有现实后果的决定；参考实现跑在 Docker 里；建议用专用 VM 或容器、最小权限。
- [S14] https://developers.openai.com/api/docs/guides/tools-computer-use.md （由 https://platform.openai.com/docs/guides/tools-computer-use 指向）：执行动作加回传截图的循环；「Run safely」一节：限制环境（隔离的浏览器或 VM、站点和动作白名单），屏幕内容不可信、不能授予权限，购买、数据传输、破坏性修改要确认，往表单里输入敏感信息也算传输，限定步数、时间、费用并核对实际结果。
- [S15] https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-securityoverview ：辅助技术应用要访问更高完整性级别的 UI，需要在 manifest 里设置 UIAccess，并由管理员启动；没有 UIAccess 的应用访问不了提升权限的 UI。
- [S16] https://gitlab.freedesktop.org/wlroots/wlroots/-/raw/master/README.md ：可组合的 Wayland 合成器模块，「约 6 万行你本来就要写的代码」；后端有 KMS/DRM、libinput、Wayland、X11、headless。
- [S17] https://doc.redox-os.org/book/graphics-windowing.html ：Orbital 是 Redox 的默认桌面环境（显示服务器、窗口管理器、合成器等）；为了避开 Wayland/X11 的依赖和 EGL/GLX，并且对不成熟的 Redox API 更简单，所以选了它；支持 Linux DRM API；vesad 写固件（UEFI/BIOS）提供的帧缓冲。
- [S18] https://fuchsia.dev/fuchsia-src/concepts/ui/scenic ：「Scenic, the Fuchsia system compositor」，是负责全局合成的系统服务；提到 Flatland 的设计目标。
- [S19] https://fuchsia.dev/reference/fidl/fuchsia.ui.composition ：Flatland 所在的 FIDL 库参考页（只确认了页面存在，以及 Flatland 相关条目）。
- [R3] 本系列第三轮报告（四个内核对象）。[R4] 本系列第四轮报告（Ed25519 确认令牌）。

- [S20] agent-first-os 公开仓库的 `linux-agent-os/specs/native-kernel-m1.md`，提交 60f1003（2026-11-02 20:18 北京时间，提交说明是 T-M1-2 接口节定稿、规格升 v0.15）：读了「接口」节的 T-M1-2 全文（调用约定、32 字节请求结构、校验顺序、错误码、限额 20/4/32、资源形态、日志格式、syscall 占用表 48–51 和 127），以及「已定：令牌签名算法为 Ed25519」和「令牌字段仍待写」两句；还有 v0.11 和 v0.15 的修订记录。
- [X] 外部参考材料（行为描述，不引用原文）：一份对闭源电脑操控产品的逆向分析材料，读了总链路文档、原生服务功能证据、按应用审批流程、确认策略全文四份。只吸收行为层面的设计思路，用自己的话重述，不引用原文，也不带入对方的任何内部名称。

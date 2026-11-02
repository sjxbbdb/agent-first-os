# 第二轮调研：完全给 agent 用的操作系统

调研时间：2026-11-02（北京时间）。只做调研，不含内核代码。
标注规则同第一轮：[R1-Sx] 指第一轮来源清单里的条目，[Tx] 是本轮新增来源（见文末），[仓库] 指本仓库，[推断] 是我的判断，没有外部来源。第一轮第四节原型（Landlock 任务沙箱）和第五节卡点已重读，本轮在它们上面接着写。

**前提变化**：从「给人用的系统上加 agent 功能」变成「系统只给 agent 用，场景全按 agent 设计」。人不再是日常用户，人只在两处出现：一是部署/管理系统的所有者，二是高风险动作的确认人。

---

## 一、第一轮结论哪些要改

| # | 原结论（第一轮） | 新结论 | 理由 | 依据 |
|---|---|---|---|---|
| 1 | 默认环境没说清，隐含有桌面和登录会话 | 默认**无桌面、无登录会话、无 shell 登录**，管理全部走 API | 没有人日常使用，桌面和登录就是纯攻击面。Bottlerocket 镜像里没有 SSH 服务甚至没有 shell，Talos 所有管理走 API、没有交互式控制台，都证明这条路在生产里能走通 | [T7][T8] |
| 2 | 缺口 7（computer-use）排在第 7，方案是 agent 专用合成器 | **降级为可选能力**，默认不装图形栈。只有任务确实要操作 GUI 程序时，在一次性 VM/沙箱里起 headless 显示 | 原生系统里 agent 通过 API、文件、命令完成工作，不需要看屏幕。computer-use 只在「必须操作别人写的 GUI 程序」时有意义，这属于兼容场景 | [推断] |
| 3 | uinput 注入可能伪造用户确认，要做可信输入隔离 | 风险**换了位置**：系统里没有用户输入设备，伪造点击的问题基本消失；但「确认」改走网络通道后，风险变成**伪造确认消息**，要用签名 token 防 | 仓库设计书已经要求「高风险授权采用一次性 token：策略层签发，内核验证；agent 不能自己生成、延长或伪造」，本项目照用，只是 token 由策略服务签发验证，不需要内核验证（见第二节） | [仓库 DESIGN_BOOK 第 174 行] + [推断] |
| 4 | 人工确认靠 Policy Firewall，形式没定 | 人工确认是**异步、带外**的：策略服务把待确认动作推到所有者的外部渠道（手机、邮件、Webhook），任务在 cgroup.freeze 状态等待，超时默认拒绝 | 没有本地屏幕，只能带外确认；等待期间必须不消耗资源也不继续执行，cgroup.freeze 正好提供整组冻结 | [R1-S4] + [推断] |
| 5 | 沙箱默认档位倾向 gVisor | 改成**两层**：整个系统本身就跑在 microVM 里（Firecracker 或 Cloud Hypervisor），VM 内每个任务再用 namespace + cgroup + Landlock；执行不可信代码的子任务才加 gVisor | 系统只给 agent 用，「整机」就是一个可丢弃的单元，外面套 VM 最省事；E2B 正是用 Firecracker microVM 跑不可信 agent 代码，暂停后从快照恢复 | [T1][T2][T11][R1-S15][R1-S16] |
| 6 | 缺口 8 快照只做文件系统 + 任务元数据，不做全系统快照 | **部分改变**：VM 内仍是这条；但在 VM 外，可以用 microVM 快照做整机快照，这在第一轮被判断为「做不到一致」，现在有了更简单的做法 | 整机快照由 VMM 做，不需要内核配合。但 Firecracker 文档明确提醒：同一快照恢复多次会导致随机数种子、熵池、加密 token 等本应唯一的状态重复；diff 快照还是 developer preview | [T3] |
| 7 | 缺口 6 恢复首选任务级日志重放，CRIU 只用于 CLI | **没变**，但多了一个选项：整机 VM 快照 | 日志重放仍是最可控的；VM 快照适合「暂停整个 agent 等待确认/空闲」 | [T3][T11][仓库 G9/G10 测试] |
| 8 | 缺口 3 权限：Landlock + BPF LSM | **没变**，并且更重要：agent 是一等公民，意味着系统里几乎所有进程都是 agent 任务，权限模型就是系统的核心 | 同第一轮 | [R1-S2][R1-S5] |
| 9 | 缺口 5 agent 间通信：用户态 broker | **没变**，优先级上调：多 agent 是默认场景 | 同第一轮；仍未找到需要内核支持的证据 | [推断] |
| 10 | 基线 6.18 longterm | **没变** | 同第一轮 | [R1-S1] |
| 11 | 内核配置：未讨论 | 新增：从最小配置出发裁剪，去掉图形、声音、输入、大部分驱动 | 内核文档提供 `make tinyconfig`（最小可能内核）和 `allnoconfig`；在 microVM 里只需 virtio 设备 | [T10] + [推断] |

**缺口优先级重排**（第一轮是平铺 8 项）：
1. 缺口 3 权限、缺口 4 沙箱（系统核心，第一轮原型已在做）
2. 缺口 1 结构化观测、缺口 2 审计回滚（agent 要靠它们自我检查）
3. 缺口 6 长任务恢复、缺口 8 快照（整机 VM 快照让这两项变便宜）
4. 缺口 5 agent 间通信
5. 缺口 7 computer-use（降为可选）

理由：原生系统里没有人在旁边盯着，「出错能被拦住、能被看到、能撤回」排在最前面 [推断]。

---

## 二、agent 原生系统最小架构草案

### 2.1 整体形态

```
宿主机（所有者管理）
└─ VMM：Firecracker 或 Cloud Hypervisor          [现有机制，T1/T2/R1-S16]
   └─ 精简 Linux 6.18（tinyconfig 起步 + virtio + Landlock + BPF LSM + cgroup v2）
      ├─ PID 1  agent-init
      ├─ taskd      任务管理器
      ├─ policyd    策略 / 审计 / 确认服务
      ├─ brokerd    agent 间消息 broker
      ├─ snapd      快照 / 回滚服务
      └─ 任务 cgroup：/sys/fs/cgroup/tasks/<task-id>/   ← agent 进程只在这里跑
```

根文件系统只读（Flatcar 的只读系统镜像 + 原子更新、Talos 的 immutable 思路）[T6][T8]，可写数据放在单独的 btrfs 卷 [推断]。

### 2.2 启动后只有这些进程

| 进程 | 职责 | 特权 | 机制来源 |
|---|---|---|---|
| agent-init（PID 1） | 挂载、建 cgroup 树、按固定顺序拉起下面四个服务、崩溃重启、收养孤儿进程 | root，但启动完成后只保留拉起服务所需能力 | 现有机制组合 [推断] |
| policyd | 加载 BPF LSM 策略；签发和校验确认 token；写 append-only 审计日志；把待确认动作推给外部渠道 | 需要 CAP_BPF/CAP_SYS_ADMIN 加载 BPF LSM 程序 | 现有机制组合 [R1-S5][R1-S7] |
| taskd | 创建/暂停/杀死任务：为每个任务建 cgroup、namespace、Landlock 规则集，起 agent 进程；汇总 cgroup.events、PSI、pidfd，给出结构化状态 | 需要管理 cgroup 和 namespace 的能力；不能加载 BPF 程序、不能改策略 | 现有机制组合 [R1-S4][R1-S10] |
| snapd | 每个任务步骤前打 btrfs 快照或开 overlay 层；回滚；可选向宿主请求 VM 快照 | 需要挂载/快照权限 | 现有机制组合 [R1-S11][R1-S12][R1-S13] |
| brokerd | 任务间消息转发，用对端凭据（SO_PEERCRED/pidfd）认证，按策略放行，消息写审计 | 普通用户 + 监听 socket | 现有机制组合 [推断] |
| agent 任务进程 | 实际执行 | 无特权；自己再叠加 Landlock 收紧 | [R1-S2] |

特权划分的原则：**能加载策略的（policyd）不能起任务，能起任务的（taskd）不能改策略**。这对应仓库的「agent 是交互层，不是信任根」[仓库]。agent 框架（模型调用、规划）本身也只是一个任务，跑在任务 cgroup 里 [仓库 DECISION-0001 不变量 1]。

### 2.3 给 agent 的接口

统一形态：**unix socket + JSON 请求**，socket 路径挂进任务 namespace，Landlock 只放行这几个 socket。理由：不用新系统调用，能做身份认证，好审计，好换实现 [推断]。只读状态同时以文件形式暴露，方便 agent 直接 cat。

| 能力 | 接口形态 | 示意 | 内核改动？ |
|---|---|---|---|
| 任务 | socket `/run/agent/task.sock`；状态文件 `/run/agent/tasks/<id>/status.json` | `{"op":"spawn","cmd":["python","x.py"],"fs":{"rw":["/work"]},"net":{"tcp_connect":[443]},"limits":{"mem":"2G","pids":64}}` → `{"task":"t42"}`；`{"op":"pause","task":"t42"}` 内部写 cgroup.freeze | 不需要：cgroup v2 + namespace + Landlock [R1-S4][R1-S2] |
| 权限 | 随任务创建时声明；任务内只能用 Landlock 进一步收紧，放宽要走确认 | `{"op":"request_grant","task":"t42","want":{"fs":{"rw":["/data"]}},"reason":"..."}` | 不需要。注意 Landlock 只能收紧不能放宽 [R1-S2]，所以「放宽」= 停掉旧任务、按新规则起新任务 |
| 快照回滚 | socket `/run/agent/snap.sock` | `{"op":"checkpoint","task":"t42","label":"before-install"}` → `{"snap":"s7"}`；`{"op":"rollback","snap":"s7"}` | 不需要：btrfs 快照 / overlayfs / VM 快照 [R1-S12][R1-S13][T3] |
| 审计 | 只读文件 `/run/agent/audit/<task>.jsonl`（append-only）+ 订阅 socket | 每行：`{"t":..,"task":"t42","hook":"file_open","path":"/etc/x","verdict":"deny"}` | 不需要：BPF LSM 产生事件 [R1-S5][R1-S7] |
| 人工确认点 | socket `/run/agent/confirm.sock`；policyd 带外推送；任务冻结等待 | `{"op":"confirm","task":"t42","action":"rm -rf /data/old","risk":"L2"}` → 冻结，返回 `{"ticket":"c9","state":"pending"}`；所有者批准后 policyd 签发一次性 token，解冻并只放行这一个动作 | 不需要：cgroup.freeze + 用户态签名 [R1-S4] |

风险等级沿用仓库设计书的 L2（大范围删除、装软件、改网络需确认）、L3（改启动链、内核、策略、密钥、发送敏感数据，强制确认或维护模式）[仓库 DESIGN_BOOK 171–172 行]。

### 2.4 哪些地方需要改内核

**结论：暂无证据需要改内核。** 上面每一块都能用现有 Linux 机制拼出来。逐个可能被提出的改动：

- **「任务」作为内核对象**：cgroup 已经是进程组 + 资源 + 冻结 + 整组杀死 [R1-S4]，暂无证据需要新对象。
- **确认 token 由内核验证**（仓库设计书的做法）：在 Linux 上可以由 policyd 校验 token 后再下发 BPF LSM 放行规则，效果等价。是否必须内核验证，取决于是否担心 policyd 被攻破；如果担心，内核也救不了（policyd 本身能加载 LSM 程序）[推断]。暂无证据需要改内核。
- **原子的「进程 + 文件」一致快照**：VM 内做不到，但 VM 快照可以替代 [T3]。暂无证据需要改内核。
- **审计防篡改**：可以把日志实时发到 VM 外（vsock 或网络）[推断]，不需要内核改动。

如果以后真要改，最可能的点是 **Landlock 缺少的规则类型**（例如它只认端口不认域名 [R1-S2]）。这类改动应当按上游 Landlock 的方式提补丁，改动面限于 security/landlock/，能否进上游这轮没有找证据，**未验证**。

### 2.5 分阶段路线

- **阶段 A（接第一轮原型 1）**：QEMU 里 Landlock 任务沙箱跑通（第一轮第四节）。
- **阶段 B（接第一轮原型 2）**：cgroup 任务载体，freeze/kill。
- **阶段 C：最小里程碑 M-A1，见下。**
- **阶段 D**：policyd 加 BPF LSM 审计和确认流程；snapd 接 btrfs 快照。
- **阶段 E**：换到 Firecracker/Cloud Hypervisor，做整机快照，处理快照唯一性问题 [T3]。
- **阶段 F**：brokerd、多 agent；computer-use 作为可选模块最后做。

### 2.6 第一个可在 QEMU 验证的最小里程碑：M-A1「无 shell 的任务机」

**目标**：系统启动后没有 shell、没有登录，只能通过 task.sock 起任务；任务被 cgroup 限资源、被 Landlock 限文件。

**设计（本轮没有运行过）**：
1. 内核：6.18 longterm，从 `make tinyconfig` [T10] 起步，加上 virtio 控制台/块设备、cgroup v2、`CONFIG_SECURITY_LANDLOCK=y` 且 `lsm=` 含 landlock（第一轮第四节已核对的要求）。tinyconfig 加完这些能不能直接启动，**未验证**，需要实测补选项。
2. initramfs 只放：静态链接的 agent-init 和 taskd（第一版可合成一个程序），不放 busybox shell。
3. QEMU 用 `-kernel -initrd`，加一个 virtio-serial 或 virtio-vsock 把 task.sock 转到宿主，宿主上用脚本发 JSON。
4. 用例：
   - spawn 一个任务，只允许写 /work，尝试写 /etc/x → EACCES（复用第一轮用例）；
   - spawn 时 limits.pids=8，任务 fork 炸弹 → 被 pids 控制器拦住；
   - pause 后进程状态冻结（cgroup.events 里 frozen 1），resume 后继续；
   - kill 后 cgroup 内无进程；
   - 在串口上尝试获取 shell → 没有可用 shell。
5. 通过标准：5 个用例输出和期望一致，串口日志里有 `landlock: Up and running.`，并记录 Landlock ABI 实测版本号。
6. 留存：config、构建日志、QEMU 命令行、串口全量日志、宿主侧请求/响应记录。

---

## 三、已有系统和项目参考

| 项目 | 做法 | 可借鉴 | 不适合 | 来源 |
|---|---|---|---|---|
| Firecracker | KVM microVM，只有 5 个模拟设备，官方称 <125ms 启动；支持快照，文档警告同一快照多次恢复会让随机数种子、熵池、加密 token 重复，diff 快照是 developer preview | 整机即可丢弃单元；快照恢复 | 需要 KVM；快照唯一性问题要自己处理 | [R1-S16][T3] |
| Cloud Hypervisor | Rust 写的 VMM，面向云工作负载，最少硬件模拟 | Firecracker 之外的备选 VMM | 这轮只读了首页，快照、热插拔等能力未核对，**未验证** | [T1] |
| Unikraft | 构建专用 OS 镜像的 unikernel 方案 | 「只放任务需要的东西」的裁剪思路 | agent 要跑任意命令、装任意包、多进程，unikernel 一般是单应用，不合适做整个系统 [推断，单应用/单地址空间细节这轮没从页面读到原文] | [T4] |
| MirageOS | 库操作系统，用 OCaml 把应用编译成独立 unikernel，跑在 Xen 或 KVM 上 | 可以作为 brokerd 这类固定网络服务的极简实现参考 | 要用 OCaml 重写应用，跑不了 agent 生成的任意代码 | [T5] |
| Flatcar | 最小、不可变、面向容器的 Linux；只读系统镜像、自动配置（Ignition）、原子更新 | 只读根 + 原子更新 + 首启配置 | 面向容器工作负载，不管任务级权限 | [T6] |
| Bottlerocket | 镜像里没有 SSH，甚至没有 shell；设置靠 API 修改并在更新中自动迁移；需要时通过 host container 带外访问 | 「无 shell、API 驱动」直接对应本项目 | 面向 AWS/容器，SSM 等组件和云厂商绑定 | [T7] |
| Talos | 面向 Kubernetes，安全、不可变、最小；所有管理走 API，无 shell 和交互式控制台；API 用 mTLS | API 唯一入口 + mTLS 认证，可直接借给 policyd/taskd 的外部管理面 | 绑死 Kubernetes | [T8] |
| Fedora CoreOS | 页面能打开，但抓到的内容里没读到有效正文 | — | — | **未找到**（页面靠 JS 渲染，没读到内容）[T9] |
| 内核最小化配置 | `make tinyconfig` 配置最小可能内核；`make allnoconfig` 全部选否 | 阶段 C 的起点 | 需要自己补回 virtio、cgroup、LSM 等，成本未评估 | [T10] |
| E2B | 开源基础设施，在云端隔离沙箱里跑 AI 生成的代码；运行时用 Firecracker microVM，从快照恢复，agent 停下就暂停；控制面和数据面分离，节点上 orchestrator 管 Firecracker 进程、网络 namespace、块设备、cgroup | 几乎是本项目「VM 外层」的现成样板；控制面/数据面分离 | 它是 agent 的**执行沙箱**，不是 agent 的操作系统；VM 内部没有本项目要的权限/审计/确认模型 | [T11][T12] |
| Modal Sandbox | 用于执行 LLM 生成代码等不可信代码的安全容器，exec 接口带 timeout，有快照功能 | `sb.exec(..., timeout=)` 这种接口形态简单，可参考 task.sock 设计 | 闭源云服务；底层是否用 gVisor 这轮没在页面上读到，**未验证** | [T13] |
| gVisor | 用户态应用内核，Sentry 截获与宿主的直接交互；安全文档按攻击面分析，并说明沙箱不能防网络可达服务等路径 | 不可信子任务的第二层隔离 | 兼容性和性能需实测；「agent 场景下怎么用」这轮未找到专门来源，**未找到** | [R1-S15][T14] |
| agent-first-os（项目负责人仓库） | 自研混合内核；agent 在 Ring 3、不是信任根；L2/L3 风险分级；确认用一次性 token，必须来自 agent 无法模拟的可信输入通道 | 风险分级、token 确认、「agent 框架不进 Ring 0」这些原则本报告全部沿用 | 自研内核本身与「基于 Linux」路线冲突（见第四节第 1 条） | [仓库] |

---

## 四、需要项目负责人拍板的事

1. **还走「基于 Linux」吗？** 倾向是：走。理由：第二节逐项看下来，agent 原生系统需要的东西都能用现有 Linux 机制拼出来，暂无证据需要改内核。自研内核的价值在于学习，不在于补 Linux 缺的能力。
2. **系统默认跑在 microVM 里吗？** 倾向是：是。理由：整机可丢弃、可快照，E2B 的生产实践就是这样 [T11]；代价是需要 KVM。
3. **完全不要 shell 和登录吗？** 倾向是：要，但保留一个**带外维护模式**（Bottlerocket 的 admin container 思路）[T7]。理由：没有任何调试入口的话，早期开发会很痛。
4. **人工确认走哪个外部渠道？** 倾向是：先用 Webhook + 签名 token，以后再接手机。理由：最简单，不绑定具体 App。这一条需要项目负责人定渠道。
5. **computer-use 降为可选**，可以接受吗？倾向是：接受。理由见第一节第 2 条。
6. **确认 token 由谁验证？** 倾向是 policyd（用户态），不做内核验证。理由见 2.4。这和仓库设计书（内核验证）不一致，需要确认。

## 五、卡点

- Fedora CoreOS 文档页靠 JS 渲染，curl 抓不到正文，所以没采用。
- E2B 文档站靠 JS 渲染，没读到内容，改用 GitHub README 代替。
- Modal 页面没读到底层隔离技术。
- Unikraft 概念页只读到片段。
- 第一轮遗留的 lore 链接和 CRIU 限制清单，这轮仍未处理。
- M-A1 只是设计，没有运行过。

## 六、未验证清单

- tinyconfig 加上必要选项后能否在 QEMU 直接启动。
- Cloud Hypervisor 的快照能力。
- Modal 是否使用 gVisor。
- gVisor 在 agent 场景下的官方用法。
- Unikraft 单应用 / 单地址空间的原文表述。
- Landlock 补丁能否进上游。
- 6.18 的 Landlock ABI 版本号（沿用第一轮）。
- 所有标 [推断] 的结论。

---

## 来源清单（本轮新增）

- [T1] https://www.cloudhypervisor.org/ ：首页，「Rust 实现的开源 VMM，面向现代云工作负载，最少硬件模拟」。
- [T2] 同 [R1-S16] Firecracker 官网，本轮没有重开，引用第一轮已读内容。
- [T3] https://raw.githubusercontent.com/firecracker-microvm/firecracker/main/docs/snapshotting/snapshot-support.md ：Firecracker 快照文档，读到了「Snapshot security and uniqueness」一节（多次恢复导致随机数种子、熵池、加密 token 重复）和「diff 快照仍是 developer preview」。
- [T4] https://unikraft.org/docs/concepts ：Unikraft 概念页，读到「目标是构建专用 OS 镜像」、unikernel 与经典宏内核的区别（只读到片段）。
- [T5] https://mirage.io/ ：MirageOS 首页，读到库操作系统、OCaml、编译成独立 unikernel 跑在 Xen/KVM。
- [T6] https://www.flatcar.org/docs/latest/ ：Flatcar 文档首页，读到「最小、不可变、面向容器」，以及只读系统镜像、原子更新、Ignition。
- [T7] https://raw.githubusercontent.com/bottlerocket-os/bottlerocket/develop/README.md ：Bottlerocket README，读到「没有 SSH 服务，甚至没有 shell」、API 改设置并随更新迁移、SSM/admin 带外访问。
- [T8] https://raw.githubusercontent.com/siderolabs/talos/main/README.md ：Talos README，读到「安全、不可变、最小」、管理全走 API、无 shell 和交互式控制台、mTLS。
- [T9] https://docs.fedoraproject.org/en-US/fedora-coreos/ ：能打开（HTTP 200），但没抓到有效正文，未采用任何结论。
- [T10] https://docs.kernel.org/admin-guide/README.html ：内核 README，读到 `make tinyconfig`（最小可能内核）、`make allnoconfig`。
- [T11] https://raw.githubusercontent.com/e2b-dev/infra/main/README.md ：E2B Runtime README，读到 Firecracker microVM 从快照恢复、跑不可信 agent 代码、agent 停下即暂停、控制面/数据面分离、orchestrator 管 Firecracker 进程/网络 namespace/块设备/cgroup。
- [T12] https://raw.githubusercontent.com/e2b-dev/E2B/main/README.md ：E2B SDK README，读到「在云端安全隔离沙箱里跑 AI 生成的代码」，以及 Sandbox.create 示例。
- [T13] https://modal.com/docs/guide/sandbox ：Modal Sandboxes 文档，读到「执行不可信代码的安全容器，适合执行 LLM 生成的代码」、带 timeout 的 exec 示例，导航里有 Snapshots。
- [T14] https://gvisor.dev/docs/architecture_guide/security/ ：gVisor 安全模型文档，读到攻击面分类、Sentry 截获与宿主的直接交互、沙箱不能防网络可达服务等路径。
- [仓库] https://github.com/sjxbbdb/agent-first-os ：本轮重新 git pull 后读了 product/docs/DESIGN_BOOK.md 第 22、42、81、171–174、318 行（所有者确认、L2/L3 分级、一次性 token、Trusted Input）。
- 第一轮来源 [R1-S1]–[R1-S20] 见 round1-agent-os-gaps.md，本轮没有重新访问。

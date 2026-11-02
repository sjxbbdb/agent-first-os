# 第一轮调研：agent 对 OS 的需求缺口与内核机制取舍

调研时间：2026-11-02（北京时间）。只做调研，不含内核代码。
依据标注：[S1] 这类编号对应文末「来源清单」；[仓库] 指老大公开仓库 sjxbbdb/agent-first-os；[推断] 是我的判断，没有外部来源。

## 〇、先说两件事

**1. 内核版本基线。** kernel.org 当前显示：最新 stable 7.3.1，mainline 7.4-rc1，longterm 6.18.60 [S1]。docs.kernel.org 页面本身标的版本也是 7.4.0-rc1 [S2]。下文提到的特性起始版本：
- Landlock：Linux 5.13 引入 [S2][S3]。ABI 已经迭代到 v12（v4 加 TCP 网络规则，v6 加 scope 信号/抽象 unix socket，v10 加 UDP，v12 加 capability 和 namespace 规则）[S2][S3]。各 ABI 版本对应哪个内核版本，我这轮没逐一核对，**未验证**。
- BPF LSM（原名 KRSI）：5.7 合入，提交 fc611f47f2188ade2b48ff6902d5cce8baac0c58「bpf: Introduce BPF_PROG_TYPE_LSM」，配置项 CONFIG_BPF_LSM [S5][S6][S7]。
- pidfd_open：Linux 5.3；PIDFD_NONBLOCK 自 5.10 [S10]。
- FICLONE/reflink ioctl：通用化于 Linux 4.5，之前是 btrfs 私有的 BTRFS_IOC_CLONE [S11]。
- io_uring：man page 确认 5.4 前要 3 次 mmap、5.11 起 SQPOLL 可非 root 使用 [S8][S9]。**io_uring 本身的起始版本这轮未从官方文档确认，未验证。**

**2. 老大仓库现状和本课题有出入。** sjxbbdb/agent-first-os 是公开的 [仓库]。读了 README、DESIGN_BOOK、DECISION-0001：它目前是**从零写的混合内核**（UEFI x86_64，QEMU+OVMF 开发，自有 capability、native syscall/IPC、Supervisor、Policy Firewall、Trusted Input），DESIGN_BOOK 明确写「Linux/Windows 兼容不是第一版目标」[仓库]。agent runtime 定在 Ring 3，基于 pi agent 改造 [仓库 DECISION-0001]。
而这次的任务描述是「Linux 内核魔改」。两条路线差别很大，见第三节第 1 条，需要拍板。下文按「魔改 Linux」来写，但仓库里已有的设计原则（agent 不是信任根、权力大但故障范围小、agent 在用户态）我照搬作为评价标准，这几条和 Linux 路线完全兼容。

---

## 一、8 个需求缺口

### 1. 结构化进程与资源可观测性
- **现状**：信息散在 /proc、/sys/fs/cgroup 的文本文件里。cgroup v2 已经有 cgroup.events（populated/frozen，可 poll 监听变化）、memory.stat、各类 *.pressure（PSI）文件 [S4]。eBPF 和 bpftrace 能做任意内核事件追踪 [S7][S14]。pidfd 解决了「PID 复用导致指错进程」的问题 [S10]。
- **痛点**：agent 要的是「某个任务（一组进程）现在在干嘛、花了多少、卡在哪」，现有接口是按进程/按文件的文本，要自己解析、拼接，格式在不同文件间不统一 [推断]。
- **改法**：不改内核。每个 agent 任务放进一个独立 cgroup，用户态 daemon 汇总 cgroup 文件 + pidfd + eBPF 事件（exec、open、connect），输出 JSON/事件流给 agent [推断，依据 S4/S7/S10 提供的原语]。
- **风险**：eBPF 程序需要特权加载；高频事件开销；/proc 字段语义跨版本小变化 [推断]。
- **怎么验证**：做一个原型，跑「编译一个项目」这样的任务，对比 agent 用原始 /proc 和用结构化接口回答「哪个子进程最耗内存、卡在什么 syscall」的准确率和 token 消耗；同时测 eBPF 追踪开启前后的编译耗时。

### 2. 可审计可回滚的操作
- **现状**：审计有 audit 子系统和 BPF LSM（KRSI 提出动机之一就是 audit 难扩展）[S6]。回滚在内核层面没有通用机制，只能靠文件系统快照/overlayfs 上层丢弃 [S12][S13]。
- **痛点**：agent 执行 rm、改配置、装包，出错后需要「撤销这一步」。文件可以靠快照，但网络请求、发出去的消息、外部 API 调用天然不可回滚 [推断]。
- **改法**：分两类。文件系统侧：任务在 overlayfs 上层或 btrfs 子卷快照上跑，成功才合并 [S12][S13]。审计侧：BPF LSM 钩子记录文件/进程/网络动作，写到 append-only 日志 [S5][S6]。不可回滚的外部副作用，只能在策略层要求人工确认（仓库里 Policy Firewall 的思路一致）[仓库]。
- **风险**：overlayfs 合并回下层不是原子操作，需要用户态实现；审计日志本身要防篡改；「能回滚」容易让人误以为外部副作用也能撤销 [推断]。
- **怎么验证**：脚本化 50 个破坏性操作（删目录、改 /etc、装包），测回滚后文件树哈希是否与操作前一致；统计审计日志对每个操作的覆盖率。

### 3. 细粒度能力与权限
- **现状**：Linux capabilities 粒度粗；Landlock 允许非特权进程给自己加限制，覆盖文件访问、TCP/UDP 端口、信号/抽象 socket scope，ABI v12 起还能限制 capability 使用和 namespace 创建 [S2][S3]。BPF LSM 可由特权方写任意策略 [S5][S7]。
- **痛点**：agent 的权限需求是按任务动态变化的（「这个任务只能写 ~/Downloads，只能连 api.x.com」），而且最好能中途收紧。Landlock 只能叠加收紧，不能放宽；它不认域名，只认端口 [S2]。
- **改法**：两层。agent 自身进程用 Landlock 自我限制（无需 root，跟上游）；系统级策略（按 cgroup 区分任务、按域名放行）用 BPF LSM + 用户态策略服务 [S2][S5]。域名级网络控制放到用户态代理做 [推断]。
- **风险**：Landlock ABI 版本跨发行版不一致，需要按文档做 best-effort 降级 [S2]；BPF LSM 策略写错会锁死系统，需要 lsm= 启动参数启用 [推断，BPF LSM 启用细节这轮未逐条核对]。
- **怎么验证**：用 Landlock 官方示例 sandboxer 起步 [S2]，写一套越权测试（读 ~/.ssh、连非白名单端口、kill 其他进程），要求 100% 被拒且有审计记录。

### 4. 沙箱隔离
- **现状**：成熟方案有三档：namespace+cgroup+seccomp（容器）；gVisor 这种用户态「应用内核」（Sentry 截获系统调用）[S15]；Firecracker 这种 KVM microVM，只有 5 个模拟设备，官方称启动 <125ms [S16]。
- **痛点**：agent 会执行不可信代码（模型生成的脚本、下载的包），容器共享宿主内核，内核漏洞就是逃逸面；VM 隔离强但和桌面/GUI 交互麻烦 [推断，依据 S15/S16 的设计说明]。
- **改法**：不需要改内核。按风险分级：读写本地文件的常规任务用 namespace+Landlock；跑不可信代码用 gVisor 或 Firecracker [S15][S16]。
- **风险**：gVisor 系统调用兼容性不全、有性能开销；Firecracker 需要 KVM，嵌套虚拟化环境里可能不可用 [推断]。
- **怎么验证**：同一批任务（pip install、跑测试、编译）在三档沙箱里跑，记成功率、耗时、内存；再拿已知容器逃逸 PoC 测试各档。

### 5. agent 间通信
- **现状**：Linux 已有 unix socket、pipe、共享内存、D-Bus（用户态）等。Landlock 的 scope 可以禁止沙箱内进程连沙箱外的抽象 unix socket、发信号 [S2]。
- **痛点**：多 agent 协作要的是「带身份、带权限、可审计的消息」，现有 IPC 不带语义身份 [推断]。
- **改法**：用户态做 broker（unix socket + SO_PEERCRED/pidfd 认证对端），不进内核 [推断]。仓库里自研内核有 native IPC 和 capability 端点，这是自研路线的优势 [仓库]。
- **风险**：broker 成为单点；做进内核会变成又一个 kdbus [推断。kdbus 历史这轮没找来源核实，**未验证**]。
- **怎么验证**：两个沙箱内 agent 只能经 broker 通信，测试绕过 broker 直连是否被 Landlock scope 拦住，测消息延迟。
- **说明**：「agent 间通信需要什么内核支持」这个问题，**未找到可靠来源**，以上全是推断。

### 6. 长任务与中断恢复（checkpoint/restore）
- **现状**：CRIU 可以冻结一个运行中的容器或单个应用，把状态存盘，再原样恢复，用于热迁移、快照等 [S17]。cgroup v2 有 cgroup.freeze 冻结整组进程、cgroup.kill 一次杀整组 [S4]。Firecracker 有 microVM 快照（官网首页提到，但这轮没细读快照文档，**细节未验证**）。
- **痛点**：agent 任务可能跑几小时，中途断电、模型超时、需要人确认。整个进程树 checkpoint 对 GUI 程序、GPU、外部 TCP 连接的支持有限 [推断。CRIU「What can be checkpointed」页面这轮打开是空页，**限制清单未验证**]。
- **改法**：分层恢复。优先做「任务级」恢复：agent runtime 自己记录步骤日志，断了从日志重放（仓库 G9/G10 测试里已有 durable checkpoint、disconnect recovery 的思路）[仓库]。进程级 CRIU 只用于纯 CLI 长任务；cgroup.freeze 用于「暂停等人确认」[S4][S17]。
- **风险**：CRIU 恢复失败的情况难预测；重放日志要求每步幂等 [推断]。
- **怎么验证**：长编译/长下载任务中途 kill -9，比较日志重放和 CRIU 恢复的成功率与耗时。

### 7. 对 computer-use 友好的设备与输入接口
- **现状**：uinput 让用户态创建虚拟输入设备注入键鼠事件，内核文档有专页 [S18]（只确认了页面存在，没细读）。虚拟显示、截图在 Linux 上主要由用户态图形栈负责（X11/Wayland 合成器），无障碍树由 AT-SPI 提供 [推断，**这两项这轮未找到并打开可靠来源**]。
- **痛点**：截图+坐标点击很脆；无障碍树质量看应用。Wayland 出于安全不允许任意程序截屏/注入，要走合成器协议 [推断，**未验证**]。
- **改法**：这基本不是内核问题。内核侧 uinput 已够用；真正要做的是一个 agent 专用的合成器/会话（headless 输出 + 结构化窗口树 + 输入注入白名单）[推断]。仓库里 Trusted Input、window/input 服务正是这一层 [仓库]。
- **风险**：uinput 注入的输入和用户真实输入难区分，提示注入后可伪造「用户确认」，需要可信输入路径隔离 [推断，仓库 Trusted Input 思路同此]。
- **怎么验证**：在 headless 会话里跑一组 GUI 任务，比较「纯截图」和「截图+无障碍树」的成功率；测试 agent 能否用 uinput 伪造确认对话框点击（应当不能）。

### 8. 低开销状态快照
- **现状**：overlayfs 把 lowerdir 和 upperdir 合并，写入只落到 upper，丢掉 upper 就等于回滚 [S12]；btrfs 有子卷快照（docs 页面存在 [S13]，内容这轮没细读）；FICLONE 在 btrfs/XFS 上做文件级 reflink [S11]；cgroup 状态是文件，可直接读出 [S4]；进程树状态靠 CRIU [S17]。
- **痛点**：要「一键回到 5 分钟前」，文件系统快照便宜，但进程状态、cgroup 设置、网络状态不在同一个快照里，没法原子一致 [推断]。
- **改法**：快照定义收窄为「文件系统 + 任务元数据」，进程状态不纳入默认快照。每步操作前打 btrfs 快照或开新 overlay 层 [S12][S13]。
- **风险**：btrfs 快照多了以后的空间和性能问题、overlayfs 上层合并的边界情况 [推断，未做实测]。
- **怎么验证**：每步一个快照、跑 1000 步，测快照创建耗时、磁盘增长、回滚耗时。

---

## 二、机制取舍表

| 机制 | 解决哪些缺口 | 改动面 | 跟上游/维护成本 | 主要风险 | 推荐程度 |
|---|---|---|---|---|---|
| eBPF（追踪） | 1、2 | 不改内核，用户态加载程序 | 低，CO-RE 可跨版本 [推断] | 需特权；verifier 限制；开销 | 强烈推荐 |
| BPF LSM | 2、3 | 不改内核，需开 CONFIG_BPF_LSM [S5] | 低 | 策略写错锁死系统；需特权 | 推荐 |
| Landlock | 3、4、5 | 不改内核，非特权可用 [S2] | 很低，上游持续加 ABI | ABI 不一致要降级；只能收紧 | 强烈推荐，第一个做 |
| cgroup v2 | 1、4、6、8 | 不改内核 [S4] | 很低 | 委派配置容易出错 | 强烈推荐，作为「任务」的载体 |
| namespace | 4、5 | 不改内核 | 很低 | 共享内核，不防内核漏洞 | 推荐，配合 Landlock |
| io_uring | 基本不解决缺口 | 不改内核 | — | 系统调用绕过 seccomp 审计的讨论一直有 [S19]；有 io_uring_disabled sysctl 可关 [S20] | 不推荐为本项目引入；沙箱里倾向关掉 |
| 新增系统调用 | 理论上 1、6、8 | 大，需要 ABI 永久维护 | 高，基本进不了上游就只能自己背 | ABI 冻结、安全审查 | 现阶段不值得做 |
| 内核模块 | 1、7 | 中 | 中到高，内部 API 不稳定 | 崩溃即内核崩溃；签名/污染内核 | 只在 eBPF 做不到时考虑 |
| 直接改内核源码 | 全部（理论上） | 最大 | 最高，每个版本 rebase | 分叉、安全补丁跟不上 | 不推荐，除非有一条上游方案确实无法满足的硬需求 |

**推荐路线：**
- **阶段 0（1–2 周）**：选 6.18 longterm [S1] 作为基线（理由：有长期补丁，Landlock、BPF LSM、cgroup v2、pidfd 全部具备），全部用现有机制。一个 agent 任务 = 一个 cgroup + 一组 namespace + Landlock 规则 + overlayfs/btrfs 快照。
- **阶段 1**：eBPF + cgroup 文件拼出结构化观测接口（缺口 1），BPF LSM 做审计日志（缺口 2）。
- **阶段 2**：沙箱分级接入 gVisor/Firecracker（缺口 4），任务级日志重放 + cgroup.freeze 做暂停恢复（缺口 6）。
- **阶段 3**：agent 专用图形会话（缺口 7，用户态工作）。
- **到这里为止零内核补丁。** 只有测出具体瓶颈（有数据）才讨论模块或补丁，补丁也要按能投上游的标准写。

**不值得做的想法：**
- 自造一个「agent 系统调用」或把 agent 通信塞进内核：成本高、上游不会收，用户态 broker 能做到同样的事 [推断]。
- 把 LLM 推理或 agent 框架放进内核：违反仓库自己的不变量「agent framework 不能进入 Ring 0」[仓库]，也无收益。
- 追求进程级「全系统原子快照」：做不到一致，文件系统快照 + 任务日志已覆盖大部分需求 [推断]。
- 用 io_uring 优化 agent 性能：agent 的瓶颈在模型调用而不是 I/O 系统调用 [推断]。

---

## 三、需要老大拍板的事

1. **路线：魔改 Linux，还是继续仓库里的自研混合内核？** 我的倾向：**两者分开**。产品可用性上走 Linux + 现有机制（上面路线），自研内核继续作为学习主线（仓库已把 learning/ 和 product/ 分开）。理由：本报告 8 个缺口在 Linux 上基本都有零补丁的解法，而自研内核要从驱动、文件系统做起，离 agent 功能很远 [仓库 + 推断]。
2. **基线内核版本**：倾向 6.18 longterm 而不是追 7.x mainline。理由：长期维护分支 [S1]；代价是拿不到最新 Landlock ABI（v12 的 capability/namespace 规则在哪个版本进的我没核实）。如果非要 v12 特性，要先查清版本。
3. **不可信代码默认跑在哪一档沙箱**：倾向默认 gVisor，GUI/重型任务例外。理由：比容器多一层系统调用拦截，又比 VM 轻 [S15][S16]，但兼容性需要实测。
4. **是否接受「外部副作用不可回滚，必须人工确认」作为产品原则**：倾向接受。理由：技术上没办法撤销已发出的网络请求，仓库的 Policy Firewall 已经是这个思路 [仓库]。

---

## 来源清单

- [S1] https://www.kernel.org/finger_banner ：kernel.org 版本横幅，stable 7.3.1、mainline 7.4-rc1、longterm 6.18.60。
- [S2] https://docs.kernel.org/userspace-api/landlock.html ：Landlock 用户态 API 文档（页面版本 7.4.0-rc1），ABI 降级示例、v2–v12 各项特性、网络/capability/namespace 规则、「first introduced in Linux 5.13」。
- [S3] https://man7.org/linux/man-pages/man7/landlock.7.html ：landlock(7)，各访问权限的起始 ABI 版本、「introduced in Linux 5.13」。
- [S4] https://docs.kernel.org/admin-guide/cgroup-v2.html ：cgroup v2 文档，cgroup.events、cgroup.freeze、cgroup.kill、pressure 文件。
- [S5] https://github.com/torvalds/linux/commit/fc611f47f2188 ：提交 fc611f47f2188ade2b48ff6902d5cce8baac0c58「bpf: Introduce BPF_PROG_TYPE_LSM」，引入 CONFIG_BPF_LSM（经搜索结果读到提交内容，附 lore 链接 https://lore.kernel.org/bpf/20200329004356.27286-2-kpsingh@chromium.org ，该 lore 链接本身我没单独打开，**未验证**）。
- [S6] https://lwn.net/Articles/798157/ 与 https://lwn.net/Articles/798918/ ：LWN 关于 KRSI 的报道和 RFC 邮件（动机：audit/LSM 扩展困难）。
- [S7] https://docs.kernel.org/bpf/prog_lsm.html ：LSM BPF Programs 文档，SEC("lsm/file_mprotect") 示例。另 https://lwn.net/Articles/821829/ ：LWN「The 5.7 kernel is out」，确认 BPF 安全钩子程序（原 KRSI）进入 5.7。
- [S8] https://man7.org/linux/man-pages/man7/io_uring.7.html ：io_uring(7)，SQ/CQ 模型，5.4 前需三次 mmap。
- [S9] https://man7.org/linux/man-pages/man2/io_uring_setup.2.html ：io_uring_setup(2)，5.11 起 SQPOLL 可非 root。
- [S10] https://man7.org/linux/man-pages/man2/pidfd_open.2.html ：pidfd_open 自 Linux 5.3，PIDFD_NONBLOCK 自 5.10。
- [S11] https://man7.org/linux/man-pages/man2/ioctl_ficlone.2.html ：FICLONE 自 Linux 4.5，前身 BTRFS_IOC_CLONE。
- [S12] https://docs.kernel.org/filesystems/overlayfs.html ：overlayfs 文档，lowerdir/upperdir/workdir。
- [S13] https://docs.kernel.org/filesystems/btrfs.html ：btrfs 文档页（只确认可访问，未细读快照章节）。
- [S14] https://bpftrace.org/docs/ ：bpftrace 文档页（只确认可访问）。
- [S15] https://gvisor.dev/docs/ ：gVisor 文档，application kernel、Sentry 截获系统调用。
- [S16] https://firecracker-microvm.github.io/ ：Firecracker 官网，KVM、5 个模拟设备、<125ms 启动。
- [S17] https://criu.org/Main_Page ：CRIU 首页，冻结并 checkpoint 容器或单个应用、恢复、热迁移。（https://criu.org/What_can_be_checkpointed 打开为空页，未采用。）
- [S18] https://docs.kernel.org/input/uinput.html ：uinput 模块文档页（只确认可访问）。
- [S19] https://lwn.net/Articles/902466/ ：LWN 关于 io_uring 与 LSM/审计钩子的讨论（评论中有对加钩子影响性能的争论）。
- [S20] https://docs.kernel.org/admin-guide/sysctl/kernel.html ：内核 sysctl 文档，含 io_uring_disabled、io_uring_group 条目。
- [仓库] https://github.com/sjxbbdb/agent-first-os ：README、product/docs/DESIGN_BOOK.md、product/docs/decisions/DECISION-0001-agent-runtime-harness.md、tests 目录文件名。

**未找到可靠来源 / 未验证：** io_uring 起始版本；Landlock 各 ABI 对应内核版本；CRIU 不支持项清单；Firecracker 快照细节；AT-SPI/无障碍、Wayland 截图与输入注入限制；kdbus 历史；agent 间通信的内核需求（全为推断）；BPF LSM 启用所需 lsm= 参数细节。

---

## 四、推荐的第一个最小原型：Landlock 任务沙箱（缺口 3、4）

**一个点**：给一个 agent 任务进程加「只能写指定目录」的文件系统限制，越权访问被内核拒绝。

**一种内核机制**：Landlock，只用最早的文件系统规则（上游 5.13 引入 [S2][S3]），不涉及网络、capability、namespace 等后期 ABI 特性，所以基线选 6.18 longterm 或任何 5.13 之后的内核都够用。零内核补丁，用户态程序调用 `landlock_create_ruleset` / `landlock_add_rule` / `landlock_restrict_self`。

**为什么先做这个**：改动面最小，非特权可用，跟上游 [S2]；同时是整个路线阶段 0 的第一块，后面的 cgroup 任务载体、审计都在它上面叠。

**内核构建要求**（来自官方文档 [S2]，已核对）：需要 `CONFIG_SECURITY_LANDLOCK=y`；还要在启动时启用，方式是 `CONFIG_LSM` 里含 `landlock`，或者内核命令行加 `lsm=landlock,...`（要把原有 LSM 列表接在后面）；启动后用 `dmesg | grep landlock` 看到 `landlock: Up and running.` 才算生效。

**QEMU 验证步骤（设计，本轮没有运行过）**：
1. 用 6.18 longterm 源码，在默认 config 基础上打开上述两项，构建内核。
2. 做一个最小 initramfs（静态链接的 busybox 加下面的测试程序），QEMU 用 `-kernel` 加 `-initrd` 加 `-append "console=ttyS0 lsm=landlock,<原有列表>"` 启动，串口输出到日志文件。
3. 测试程序（C，约 100 行，可参考官方文档的 sandboxer 示例 [S2]）：先查询 ABI 版本，再创建规则集，只允许读 `/` 下只读目录、读写 `/tmp/work`，然后 restrict_self，再依次尝试：写 `/tmp/work/a`（期望成功）、写 `/etc/x`（期望 EACCES）、读 `/root/secret`（期望 EACCES，除非规则允许读）、通过 `execve` 起的子进程同样被限制（期望继承）。
4. 通过标准：串口日志里出现 `landlock: Up and running.`，4 条用例输出和期望一致；对照组（不加 restrict_self）里写 `/etc/x` 成功，证明限制确实是 Landlock 造成的。
5. 日志留存：构建命令、完整构建日志、QEMU 命令行、串口全量日志、测试程序源码和输出，交验收审查复核。

**风险**：上游 Landlock 对 `/proc`、`/dev` 等特殊文件系统的限制有细节，用例先避开；不同发行版 ABI 版本不一，程序要按官方示例做 best-effort 降级 [S2]。
**未验证**：6.18 的具体 Landlock ABI 版本号（文档只列出了各 ABI 的功能，没在页面里给出内核版本对照，未找到可靠来源），所以原型里先用 ABI 版本查询接口，把实测返回值记入日志，不事先假定。

**后续一步（原型 2）**：给任务建 cgroup v2 子组，把进程放进去，验证 `cgroup.freeze` 暂停和 `cgroup.kill` 整组杀死 [S4]。同样在 QEMU 里跑，同样留日志。

## 五、本报告卡点

- criu.org 的「What can be checkpointed」页打开为空页，CRIU 的限制清单没有采用。
- BPF LSM 提交里附的 lore.kernel.org 链接没有单独打开，仅凭 GitHub 上的提交页内容。
- 本节第四部分的 QEMU 验证只是方案，没有真实运行。需要 codex工具人 执行构建和运行后才算数。

# 规格：原型3 M-A1「无 shell 的任务机」
版本：v0.1　日期：2026-11-02　状态：草稿，三项待拍板

来源：`research/round2-agent-native-os.md` 第二节第六小节（M-A1）及第二节 2.2、2.3。文中 [T3]、[T10] 等编号沿用该调研的来源清单，[R1-Sx] 指第一轮来源清单。本规格不新增调研结论，调研标为「未验证」或「[推断]」的内容这里同样标注。

## 已定与待拍板
已定（项目负责人确认，2026-11-02，见 `decisions/0001-route-and-side-effects.md`）：
| 项 | 结论 |
|----|------|
| 路线 | 只做 Linux 魔改 |
| 外部副作用 | 一律人工确认。M-A1 不实现确认流程，只保证后续能接入 |

待拍板（项目负责人尚未决定，以下倾向不得当成已定）：
| 项 | 调研倾向 | 本规格暂用 | 对 M-A1 的影响 | 状态 |
|----|----------|------------|----------------|------|
| 系统默认跑在 microVM 里 | 是（Firecracker 或 Cloud Hypervisor），代价是需要 KVM | M-A1 在 QEMU 里验证，不引入 microVM | 阶段 E 才换 microVM，M-A1 不受影响；QEMU 里是否启用 KVM 加速不影响用例结果 | 待拍板 |
| 完全不要 shell 和登录，但保留带外维护模式 | 要，维护模式参考 Bottlerocket admin container 思路 [T7] | 默认镜像不含 shell、无登录；维护模式本版不实现，调试靠串口日志和 task.sock | 开发期无调试入口，排障只能靠日志。是否另做维护镜像由拍板决定 | 待拍板 |
| 人工确认走哪个外部渠道 | 先 Webhook 加签名 token，以后再接手机 | 本版不涉及 | 无影响；确认流程在阶段 D | 待拍板 |

调研第四节另有两项也未定，M-A1 不依赖：computer-use 降为可选；确认 token 由 policyd 在用户态验证（与仓库设计书的内核验证不一致）。基线版本 6.18 longterm 与沙箱档位沿用 `specs/proto1-landlock-task-sandbox.md` 的待拍板状态。

## 目标
系统启动后没有 shell、没有登录，只能通过 task.sock 起任务；任务被 cgroup 限资源、被 Landlock 限文件。

## 范围
做什么：
- 内核：6.18 longterm，从 `make tinyconfig` 起步 [T10]，补 virtio 控制台与块设备、cgroup v2（含 pids 与 freezer 能力）、`CONFIG_SECURITY_LANDLOCK=y`，且 `lsm=` 含 landlock。
- initramfs 只放静态链接的 agent-init 与 taskd（第一版可合成一个程序），不放 busybox 或任何 shell。
- QEMU 用 `-kernel`、`-initrd`，加 virtio-serial 或 virtio-vsock 把 task.sock 转到宿主，宿主上用脚本发 JSON。
- 在 QEMU 里验证，不碰真机。

不做什么：
- 不做 policyd、snapd、brokerd，不做 BPF LSM、审计、确认流程、快照。
- 不做 microVM（Firecracker、Cloud Hypervisor）。
- 不做维护模式、不做 computer-use。
- 不改内核源码；taskd 与 agent-init 是用户态程序。

## 接口
对宿主侧脚本暴露的接口只有 task 一类，形态沿用调研 2.3：unix socket 加 JSON 请求。M-A1 只实现以下操作：
- `spawn`：参数含命令、文件系统读写目录、资源限制（至少 `pids`）。示意（来自调研 2.3）：`{"op":"spawn","cmd":[...],"fs":{"rw":["/work"]},"limits":{"pids":8}}`，返回 `{"task":"<id>"}`。
- `pause`：内部写 `cgroup.freeze`。
- `resume`：解冻。
- `kill`：内部写 `cgroup.kill`。
- 状态：`/run/agent/tasks/<id>/status.json` 只读文件。字段含义本版仅要求能反映任务存活与冻结状态，其余字段由 T5 实施时与总经理确认。

调研 2.3 示例里的 `net` 字段不在 M-A1 范围内。接口细节（JSON 字段名、错误码）调研只给了示意，没有完整规格，T5 实施前需要定稿，见下方「未决细节」。

内核与启动侧要求：
- 串口日志出现 `landlock: Up and running.`。
- 记录 Landlock ABI 实测版本号（调研：6.18 的具体 ABI 号未找到可靠来源，以实测为准）。

## 测试用例
| 编号 | 动作 | 期望 |
|------|------|------|
| U1 | spawn 一个任务，只允许写 `/work`，任务内尝试写 `/etc/x` | 失败，errno 为 EACCES |
| U2 | spawn 时 `limits.pids=8`，任务内跑 fork 炸弹 | 被 pids 控制器拦住，任务内进程数不超过 8，系统不崩溃 |
| U3 | pause 后检查，再 resume | pause 后 `cgroup.events` 里 `frozen 1`，进程不再运行；resume 后继续运行 |
| U4 | kill | cgroup 内无进程 |
| U5 | 在串口上尝试获取 shell | 没有可用 shell |

U5 的判定写具体：一是 initramfs 内容清单（cpio 列表）里没有 shell 可执行文件；二是串口上输入回车和常见命令没有任何提示符或响应。两条都要给证据。调研只写了「没有可用 shell」，没有定义判据，这是本规格补的，需总经理确认。

## 通过标准
- 5 个用例输出与期望一致。
- 串口日志里有 `landlock: Up and running.`。
- Landlock ABI 实测版本号已记入日志。
- 验收审查复核送审材料齐全并复跑通过。

## 风险
- tinyconfig 加完上述选项能否直接启动，调研标为「未验证」，需要实测补选项，补了哪些要在 config 差异里列出。
- fork 炸弹用例可能把 QEMU 内的小内存环境拖垮，需要给用例设置内存上限并写明。
- task.sock 经 virtio-serial 或 vsock 转到宿主，两种方式调研没有比较，二选一由 T5 实测后记录理由。
- 没有 shell 意味着排障只能靠串口日志，T5、T6 的日志要比平时更完整。
- 本规格的 QEMU 步骤只是设计，没有运行过，运行证据靠 T6。

## 未决细节（T5 实施前定稿）
- task.sock 与 JSON 字段名、错误码的完整定义。
- 转发 task.sock 用 virtio-serial 还是 vsock。
- fork 炸弹用例的内存上限。

## 验证方法
送审材料清单（缺一项即判不通过）：
- 补丁或提交的位置：本原型零内核补丁，写明 agent-init、taskd、构建脚本、宿主脚本的提交位置。
- 内核版本与 config：6.18 longterm 的具体小版本号，完整 `.config` 以及相对 tinyconfig 的补选项差异。
- 完整构建命令与构建日志：内核、agent-init 与 taskd、initramfs 各一份，附 initramfs 的 cpio 内容清单。
- QEMU 启动命令与串口日志：串口全量日志落文件，另附宿主侧每次请求与响应的记录。
- 测试用例与输出：U1 到 U5 的脚本与输出。
- GPL 声明与改动文件清单：内核未改动要明说；agent-init 与 taskd 的许可证写明；initramfs 不含 busybox，如含其他 GPL 组件需说明。
- 调研与规格结论的真实来源链接：见调研来源清单。

复现要求：别人拿同样的材料在盒子上能原样复跑，得到同样的串口输出。

## 依赖的决定与调研
- `research/round2-agent-native-os.md`（第二节 2.2、2.3、第六小节，第四节待拍板事项）。
- `decisions/0001-route-and-side-effects.md`。
- 前置原型：`specs/proto1-landlock-task-sandbox.md`（U1 复用其用例思路）。

## 变更记录
- v0.1 2026-11-02 初稿，依据第二轮调研第二节第六小节。

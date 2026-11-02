# 任务清单：原型1 Landlock 任务沙箱、原型2 cgroup 冻结、原型3 无 shell 任务机
版本：v0.5　日期：2026-11-02
规格：`specs/proto1-landlock-task-sandbox.md`（T1 到 T4）、`specs/proto3-agent-native-taskbox.md`（T5 到 T7）
说明：路线已定为只做 Linux 魔改（见 `decisions/0001-route-and-side-effects.md`）。基线版本（暂用 6.18 longterm）和沙箱档位仍待项目负责人拍板，基线若改，T1 需按新版本重做。T4 只有大纲，细化等 T3 通过后另写规格。T5 到 T7 对应原型3，microVM 默认、开发阶段保留 shell、确认渠道已由决定 0002 确认；「稳定」判定标准等仍待拍板，不影响 T5 到 T7 的执行。

| ID | 任务 | 负责人 | 输入 | 验收标准 | 依赖 |
|----|------|--------|------|----------|------|
| T1 | 构建内核与 initramfs | codex工具人 | 规格第「接口」节；6.18 longterm 源码（来自 kernel.org，记录下载地址、小版本号与校验和）；静态 busybox | 1. 内核构建成功，`.config` 含 `CONFIG_SECURITY_LANDLOCK=y` 并按规格启用 landlock；2. initramfs 能被该内核启动并进入 shell；3. 留存构建命令、完整构建日志、`.config`、内核版本号、校验和；4. 构建步骤可由别人原样复跑 | 无 |
| T2 | 测试程序与 QEMU 运行 | codex工具人 | T1 的内核与 initramfs；规格「测试用例」节 | 1. 测试程序源码入库，静态编译成功；2. QEMU 按规格命令启动，串口全量日志落文件，含 `landlock: Up and running.`；3. C0 到 C4 输出与期望逐条一致；4. ABI 查询返回值记入日志；5. 留存 QEMU 命令行、串口日志、测试输出、改动文件清单与 GPL 说明；6. 任一用例与期望不符时如实报告，不得改期望凑通过 | T1 |
| T3 | 验收 | 验收审查 | T1、T2 的全部送审材料；规格「验证方法」送审清单 | 结论为通过、不通过或有条件通过之一，附依据、复现步骤、缺陷清单；材料缺项直接判不通过；验收审查自行复跑一次并附复跑日志 | T2 |
| T4 | 原型2：cgroup v2 `cgroup.freeze` 与 `cgroup.kill` | 待定（先规格，再由 codex工具人实施，验收审查验收） | T3 结论为通过；调研第一轮第四节「后续一步」和 [S4] | 大纲：在 QEMU 内建 cgroup v2 子组，放入进程，验证冻结后进程不再运行、解冻后恢复，`cgroup.kill` 能杀掉整组；同样留构建与串口日志。细化的验收标准待 T3 通过后补 | T3 通过 |
| T5 | 原型3 M-A1 构建 | codex工具人 | `specs/proto3-agent-native-taskbox.md`；6.18 longterm 源码（记录下载地址、小版本号与校验和）；T3 通过的结论 | 1. 内核从 tinyconfig 起步构建成功，`.config` 含 cgroup v2、`CONFIG_SECURITY_LANDLOCK=y`、virtio 控制台与块设备，并列出相对 tinyconfig 的补选项差异；2. agent-init 与 taskd 静态编译成功，源码入库；3. 构建开发配置（含静态 busybox）的 initramfs，附 cpio 内容清单；同时提供稳定配置（不含 busybox 或任何 shell）的构建开关并至少构建一次，附 cpio 清单；4. 实施前先把规格「未决细节」三项（JSON 字段名与错误码、virtio-serial 或 vsock、fork 炸弹内存上限）写成方案交总经理定，定稿后记录并按定稿实施，不得自行拍板；5. 留存构建命令、完整构建日志、内核版本号、校验和；6. 构建步骤可由别人原样复跑 | T3 通过 |
| T6 | 原型3 M-A1 QEMU 验证 | codex工具人 | T5 的内核与 initramfs；规格「测试用例」节（开发配置与稳定配置） | 1. QEMU 按记录的命令启动，串口全量日志落文件，含 `landlock: Up and running.`，并记录 Landlock ABI 实测版本号；2. 宿主侧脚本经 task.sock 发 JSON，每次请求与响应落文件；3. 在开发配置上 U1 到 U4 输出与期望逐条一致；U5 在开发配置下可选，只记录能进 shell；稳定配置上另跑 U5，给出 cpio 清单无 shell 与串口无响应两条证据；4. 留存 QEMU 命令行、串口日志、请求响应记录、测试输出、改动文件清单与 GPL 说明；5. 任一用例与期望不符时如实报告，不得改期望凑通过 | T5 |
| T7 | 原型3 M-A1 验收 | 验收审查 | T5、T6 的全部送审材料；规格「验证方法」送审清单 | 结论为通过、不通过或有条件通过之一，附依据、复现步骤、缺陷清单；材料缺项直接判不通过；验收审查自行复跑一次并附复跑日志 | T6 |

## 依赖图
T1 → T2 → T3 → T4
T3 → T5 → T6 → T7（T5 起依赖 T3 通过，与 T4 并行）

## 状态
| ID | 状态 | 更新 |
|----|------|------|
| T1 | 未开始 | 2026-11-02 |
| T2 | 源码已入库（本地提交） | 2026-11-02 |
| T3 | 未开始 | 2026-11-02 |
| T4 | 未开始，等 T3 | 2026-11-02 |
| T5 | 未开始，等 T3 | 2026-11-02 |
| T6 | 未开始 | 2026-11-02 |
| T7 | 未开始 | 2026-11-02 |

## 备注
M-A1 的 U3、U4 用到 cgroup.freeze 与 cgroup.kill，T4 正是验证这两项。按总经理要求 T5 到 T7 只依赖 T3；T4 若先做完，T6 可复用其结论，但 T6 仍要自己跑通 U3、U4，不能引用 T4 的结果代替。

## 路线变更记录
2026-11-02：项目负责人确认路线改为自研内核，Linux 线暂停，原型1 到此收尾。T2 的测试程序、脚本、文档和日志已入库于 `prototypes/proto1-landlock/`（不含任何二进制，二进制的哈希与获取方式见其 `artifacts/SHA256SUMS-binaries.txt`）。T3 验收及 T4 到 T7 随 Linux 线暂停，不再推进；此前的决定 0001 中「只做 Linux 魔改」一条已被新路线取代，已由 `decisions/0003-self-built-kernel-supersedes-linux-route.md` 正式取代。

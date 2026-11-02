# 规格：原型1 Landlock 任务沙箱
版本：v0.2　日期：2026-11-02　状态：草稿，基线版本与沙箱档位待拍板

来源：`research/round1-agent-os-gaps.md` 第四节（缺口 3、4）。文中 [S2][S3] 等编号沿用该调研的来源清单。本规格不新增调研结论，凡调研标为「未验证」的内容这里同样标为未验证。

## 已定与待拍板
已定（项目负责人确认，2026-11-02，见 `decisions/0001-route-and-side-effects.md`）：
| 项 | 结论 |
|----|------|
| 路线 | 只做 Linux 魔改，自研内核暂不动 |
| 外部副作用（已发出的网络请求、消息） | 一律人工确认，作为产品原则；本原型不涉及 |

待拍板（项目负责人尚未决定，以下内容不得当成已定）：
| 项 | 调研倾向 | 本规格暂用 | 状态 |
|----|----------|------------|------|
| 基线内核版本 | 6.18 longterm，不追 7.x | 6.18 longterm | 待拍板 |
| 不可信代码默认沙箱档位（容器 / gVisor / Firecracker） | 默认 gVisor | 本原型不涉及，只做 Landlock 一层 | 待拍板 |

## 目标
证明一件事：非特权进程可以给自己加「只能写指定目录」的文件系统限制，越权访问被内核拒绝，限制能被子进程继承。这是路线阶段 0 的第一块，后续 cgroup 任务载体和审计都叠在它上面。

## 范围
做什么：
- 只用 Landlock 最早的文件系统规则（上游 5.13 引入 [S2][S3]）。
- 零内核补丁，只调用 `landlock_create_ruleset`、`landlock_add_rule`、`landlock_restrict_self` 三个系统调用。
- 在 QEMU 虚拟机里验证，不碰真机。

不做什么：
- 不涉及 Landlock 网络规则、capability 规则、namespace 规则、scope 等后期 ABI 特性。
- 不涉及 BPF LSM、cgroup、gVisor、Firecracker、overlayfs 快照。
- 不改内核源码，不写内核模块。
- 不做域名级网络控制。

## 接口
内核侧：
- `CONFIG_SECURITY_LANDLOCK=y`。
- 启用方式二选一：`CONFIG_LSM` 含 `landlock`，或内核命令行 `lsm=landlock,<原有 LSM 列表>`（原有列表接在后面）。依据：调研第四节，源自 [S2]。
- 生效判据：串口日志出现 `landlock: Up and running.`。

用户态测试程序（C，约 100 行，参考 [S2] 的 sandboxer 示例）：
- 启动先查询 Landlock ABI 版本，把返回值写进日志，不事先假定 6.18 的 ABI 号。
- 规则集（本规格对调研措辞做了收紧，见下方「与调研报告的差异」）：
  - 只读：initramfs 里测试程序运行所需的目录（如 `/bin`、`/lib`、`/usr`，以实际 initramfs 为准）。
  - 读写：`/tmp/work`。
  - 其余路径（包括 `/etc`、`/root`）不授予任何权限。
- `restrict_self` 之后依次执行用例，输出每条用例的期望与实际。
- 按 [S2] 做 best-effort 降级：ABI 低于所需版本时按文档示例降级，并在日志记录降级发生与否。

与调研报告的差异：报告写「只允许读 `/` 下只读目录」，字面上 `/` 被授予读权限的话，`/root/secret` 的读会成功，与期望 EACCES 冲突。本规格明确为只给 initramfs 运行所需的具体目录读权限，不授予 `/` 整体。这一处需总经理确认。

## 测试用例
| 编号 | 动作 | 期望 |
|------|------|------|
| C1 | 写 `/tmp/work/a` | 成功 |
| C2 | 写 `/etc/x` | 失败，errno 为 EACCES |
| C3 | 读 `/root/secret`（事先在 initramfs 里放好） | 失败，errno 为 EACCES |
| C4 | 通过 `execve` 起子进程，子进程重复 C2 | 失败，errno 为 EACCES（限制被继承） |
| C0 对照 | 不调用 `restrict_self`，写 `/etc/x` | 成功，证明 C2 的拒绝来自 Landlock |

## 通过标准
- 串口日志里有 `landlock: Up and running.`。
- C1 到 C4 的实际结果与期望一致，C0 对照写入成功。
- ABI 查询的实际返回值已记入日志。
- 验收审查复核送审材料齐全并复跑通过。

## 风险
- 上游 Landlock 对 `/proc`、`/dev` 等特殊文件系统的限制有细节，用例先避开。
- 不同发行版 ABI 版本不一，程序必须按 [S2] 做降级，不能硬编码 ABI。
- 6.18 的具体 Landlock ABI 版本号未找到可靠来源（调研已声明），以实测返回值为准。
- 调研第四节的 QEMU 步骤只是设计，没有运行过，本规格的步骤同样是设计，运行证据要靠 T2。

## 验证方法
送审材料清单（缺一项即判不通过）：
- 补丁或提交的位置：本原型零内核补丁，写明测试程序和构建脚本的提交位置。
- 内核版本与 config：6.18 longterm 的具体小版本号，完整 `.config` 或相对默认 config 的差异。
- 完整构建命令与构建日志：内核、busybox、initramfs、测试程序各一份。
- QEMU 启动命令与串口日志：`-kernel`、`-initrd`、`-append "console=ttyS0 lsm=landlock,<原有列表>"`，串口全量日志落文件。
- 测试用例与输出：C0 到 C4 的源码与输出。
- GPL 声明与改动文件清单：内核未改动要明说；测试程序若自带许可证需写明；静态链接 busybox 的 GPL 义务一并说明。
- 调研与规格结论的真实来源链接：见调研来源清单。

复现要求：别人拿同样的材料在盒子上能原样复跑，得到同样的串口输出。

## 依赖的决定与调研
- `research/round1-agent-os-gaps.md`（第三节待拍板事项、第四节原型设计）。
- 决定记录：`decisions/0001-route-and-side-effects.md`。

## 变更记录
- v0.1 2026-11-02 初稿，依据第一轮调研第四节。
- v0.2 2026-11-02 路线与外部副作用原则改为已定（决定 0001），基线版本与沙箱档位仍待拍板。

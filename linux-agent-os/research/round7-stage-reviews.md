# 阶段审查记录（round7）：T-M1-0 第 1 到 4 轮与 T-M1-1

## 一、范围与状态
- 本文汇总验收审查对 T-M1-0（四轮）和 T-M1-1 的阶段审查记录，结论保持原文。数据记录拆成五个文件 `research/round7-review-data-*.md`，日志没有入库，只记了 sha256（`research/round7-review-logs-sha256.txt`）。
- 全部是 TCG 下的阶段结论，KVM 与其他 OVMF 未测，不带入 T-M1-5。
- 各记录里「报告」「REVIEW-DATA.md」「本目录」指的是对应轮次的记录与数据文件，对应关系：第 1 轮到第 3 轮各自的数据文件是 `round7-review-data-t-m1-0-r1.md` 到 `-r3.md`，第 4 轮（A）是 `round7-review-data-t-m1-0-r4.md`，T-M1-1（B、C）是 `round7-review-data-t-m1-1.md`。
- **T-M1-1 原结论：有条件通过，条件为补 5a 批准记录，已补（2026-11-02）**（经总经理转达，记录见任务清单 T-M1-1 第 5a 条和规格 V2）。
- 时间线说明（入库时补，不改原文）：第 4 轮记录里「LICENSE 只在本地 main 的 599e2da 上未推送」已过时，599e2da 之后已推送到远端；T-M1-0 的重要项和几条次要项已转 codex工具人 修，是否已修未核实，修复提交出来后更新任务清单状态表。

## 二、入库时做的替换（只有这些，结论与措辞没有改）
- 工作路径换成尖括号占位：评估目录、T-M1-0 复核目录、T-M1-1 复核目录、规格仓库、临时目录、工作区。
- 构建用户名（原文是一个词，出现在属主、`/home` 路径排除条件等处）换成「构建用户」，原来的主目录路径换成 `<构建用户主目录>`。
- 项目负责人的称呼按本仓库的规则统一写成「项目负责人」；GitHub 账号名换成「仓库所有者的账号」。
- 第三方公开地址、提交号、哈希原样保留。记录里的代码提交作者 `afos-codex <afos-codex@localhost>` 是匿名的构建作者标识，不含个人信息，保留。

# 记录 T-M1-0 第 1 轮审查（送审提交 f66c5d7）

（以下为验收审查原文，只做第二节说明的路径与账号替换，标题层级下移一级）

## T-M1-0 阶段审查（验收审查）

日期：2026-11-02　送审提交：work 仓库 f66c5d7（含 b4496fc），基于 0066bee
性质：阶段审查。结论不带入 T-M1-5 的最终冻结版本。证据全部是 TCG（QEMU 10.0.13，OVMF 2025.02 Debian）下的结果，KVM、其他 OVMF、真机固件未测。

### 结论：有条件通过
构建、启动、冻结、回归四项我独立复现成功。条件是下面第 1、2 号缺陷要在进入 T-M1-1 之前处理。

### 依据（我亲自跑的）
- 两个不同路径的干净克隆（HEAD=f66c5d7，工作区干净），不做手工 chmod 或 mkdir，原样执行 `bash product/tools/build-m1-default.sh`，退出码 0。镜像 sha256 为 15ec6371…80ef，与冻结目录一致；BOOTX64.EFI、kernel.elf、initrd.bin 也一致；同路径重建一致；产物里无时间戳；断网（`unshare -rn`）构建一致。
- `run-m1-uefi.sh` 启动 4 次，串口日志字节一致，也与冻结的 clean-c/run-001 一致。串口含 `AGENTOS UEFI EXIT BOOT SERVICES`、`KERNEL C OK`、`AFOS-M1 version=0.1.0-dev build=f66c5d7`。日志头满足门槛 8 的各字段。
- 冻结目录：`sha256sum -c` 134 条全过，没有清单外文件，两个补丁依次应用到 0066bee 后树与 f66c5d7 一致。
- 回归 95 项我在干净克隆里重跑：86 通过、9 失败，失败的是 8 项 virtio/g7 加 g2-irq，与 round5 和送审结果逐项一致，无新增失败。
- 姓名、拼音、非 构建用户 的 /home 路径 0 命中。没有改测试断言。
- 许可证（门槛 6）部分满足，与报告自述一致：新增三个文件为 `LicenseRef-AgentFirstOS-TBD`，仓库根目录无 LICENSE。BOOTX64.EFI 只用 clang 与 lld-link 链出，不依赖 gnu-efi，镜像里不含 OVMF。
- 我自己核对了源码：`tss_init()` 在 kernel.c:1518，位于 `#if defined(AGENT_OS_TEST_RING3)` 内；`load_kernel_gdt` 的调用在 363 行，由 `tss_init` 引出。所以报告里「默认启动执行了 tss_init / load_kernel_gdt」不成立。测试宏我数出 46 个，报告写 45。

### 复现步骤
```
. <评估目录>/env.sh
export AFOS_OVMF_CODE=<OVMF_CODE_4M.fd> AFOS_OVMF_VARS=<OVMF_VARS_4M.fd> AFOS_OVMF_VERSION="Debian ovmf 2025.02-8+deb13u1"
git clone <评估目录>/m1-t0/work src && cd src && git checkout f66c5d7
bash product/tools/build-m1-default.sh
sha256sum build/m1-default/afos-m1-uefi.img   # 期望 15ec63714cd8831fe0c2f8f831d1636239b34ecfee059b1771f186ebf8b080ef
bash product/tools/run-m1-uefi.sh
cd <冻结目录> && sha256sum -c SHA256SUMS
```
完整命令与输出：`REVIEW-DATA.md`、`logs/`，哈希见 `SHA256SUMS`。

### 缺陷清单
1. 重要：功能清单把 GDT/TSS 初始化算作默认启动已执行，实际只在测试宏内（kernel.c:1517-1518、363）。
2. 重要：构建脚本只清了仓库自己的环境变量，工具自带的变量能把测试宏注进「默认」镜像。实测 `NASMENV=-D…` 后镜像 sha256 变成 d00e1378…，kernel.elf、initrd.bin 随之改变，串口版本行仍是 `build=f66c5d7`（logs/item7a-inject.txt）。这直接威胁门槛 11 的含义。gcc 的 CPATH、clang 的 CCC_OVERRIDE_OPTIONS 等同类变量未逐一实测。
3. 次要偏重要：清单漏写 vector 0x80 的 DPL3 门、vector 32 的桩，以及 initrd 与 kernel.elf 里内嵌、但不执行的测试用户程序和测试输出串。
4. 次要：测试宏数量 46，不是 45。
5. 次要：「调用点都在 1517-1827 行」对 syscall 路径不成立，kernel.c:1344-1350 的 `schedule_into_next` 没有宏保护。
6. 次要（门槛 4）：9 项失败里 7 项的冻结日志是 0 字节，实际失败输出在 build/ 下没有冻结。
7. 次要：b4496fc 多改了 linux-agent-os/prototypes/proto1-landlock/env.sh 的模式（644 到 755），与任务无关，与「只改了产品代码」不一致。
8. 次要：冻结只靠 `chmod a-w`，属主可改回。
9. 次要：日志头里的 ovmf_version 是环境变量手填，kernel_version 取自 build-info.txt，不是启动时实测；构建日志的 cmd 与 exit 行由仓库外的包装脚本打印。
10. 次要：缺工具时（未加载 env.sh）脚本直接报 127 `nasm: command not found`，没有提示。
11. 自查表夸大：T-M1-0 第 5 条与门槛 4 写「满足」，按以上应改为「部分满足」。

### 修改意见（由总经理转给 codex工具人）
- 修正功能清单：删掉 GDT/TSS 的「已执行」说法，补上缺陷 3、4、5 的内容；自查表第 5 条与门槛 4 改为部分满足。
- 构建脚本在调用工具前清除或固定 `NASMENV`、`CPATH`、`C_INCLUDE_PATH`、`CCC_OVERRIDE_OPTIONS`、`CFLAGS`、`LDFLAGS` 等会影响产物的变量；或者把「构建时环境变量的快照」与「kernel.elf 内测试宏是否为空」写进 build-info.txt 并冻结。改完后重做冻结，递增版本，重跑第 2 条的注入实验确认被挡住。
- 7 项 0 字节失败日志补上 build/ 下的实际输出。
- 缺陷 7 的无关改动回退。

### 没核的
其他编译器环境变量的注入、反汇编确认默认镜像机器码里无 `sti`、loader 在 ExitBootServices 前后对 IF 的处理、UEFI 标记为什么打印两次、冻结目录里早期失败尝试日志的细读，以及 KVM、其他 OVMF、真机。

---

# 记录 T-M1-0 第 2 轮审查（修订版 r2，提交 a61e8f8 与 428d604）

（以下为验收审查原文，只做第二节说明的路径与账号替换，标题层级下移一级）

## T-M1-0 修订版（r2）复核（验收审查）

日期：2026-11-02　送审：work 仓库分支 t-m1-0-r2，提交 a61e8f8 与 428d604，新冻结目录 freeze/afos-m1-t0-0.1.0-dev-428d604。TCG 下验证，KVM、其他 OVMF、真机未测。

### 结论：有条件通过（阶段结论，不带入 T-M1-5）
上一轮 11 条缺陷里，功能清单（A）和多数次要项关闭。环境变量净化只做到防误设，没有做到防绕过，另发现一个测试脚本在干净克隆里失败。这是第二轮，若第三轮仍留同一问题，我按规则升级给总经理处理。

### 依据（亲自跑的）
- 干净克隆两个路径、原地重建、`unshare -rn` 断网构建，镜像、BOOTX64.EFI、kernel.elf、initrd 与新冻结一致；启动 3 次串口字节一致且与冻结一致，含 `build=428d604`；冻结 255 条全过，SHA256SUMS 自身 sha256 为 65dcb207…ef13，无清单外文件，补丁应用后树等于 428d604。
- 与旧冻结相比：kernel.elf 只差 7 字节（版本串里的构建号），.text 的 `objdump -d` 全文无差异；BOOTX64.EFI 变了，因为新增打印固件信息的函数。
- 功能清单：内核源码在 f66c5d7 到 428d604 之间没变，更正的各句按源码与符号表核到。仅措辞问题：结尾是 `hlt`（IF 由入口 cli 清掉），不是「cli; hlt」，第一条 cli 在 `__kernel_text_start`。
- 反汇编：kernel.elf 里 sti 0 条、cli 4 条（`__kernel_text_start`、`enter_user_mode`、`scheduler_halt`、`kernel_exception_handler`）；BOOTX64.EFI 两者都是 0。UEFI 标记打两次，因为 `report()` 先写串口再写 ConOut（uefi_loader.c:331-335），「OVMF 把 ConOut 转到串口」是推断，没查证。
- 注入实验：正常调用下 20 个变量逐个单独设置，哈希都等于基线。

### 缺陷清单
1. 重要：净化可被绕过。脚本只判断 `AFOS_M1_SANITIZED` 是否为 1（build-m1-default.sh:12），调用方先设它，就跳过 `env -i`；此时 `NASMENV`、`CPATH` 注入都改变产物（镜像哈希变为 600ff988…、ada04f42…），版本行仍是 `build=428d604`，build-info 仍写「env -i」（logs/B-inject.txt、B-inject2.txt）。
2. 重要：PATH 前面放同名假 nasm 能改产物，只在 build-info 的 `tool_nasm` 里看得出来。
3. 重要：`g6-mmio-map-test.sh` 往 `build/` 写文件却不建目录，在没有 build/ 的干净克隆里必然失败（链接报错）。我的 95 项重跑是 85 过 10 败，多出的就是它，单独重跑通过，其余与 round5、两个冻结版一致。送审方先做了默认构建再跑回归，没暴露。
4. 次要：build-info 里的 `test_macro_hooks` 取的是外层环境变量，在净化路径下必为空，不是检查；`build_env=env -i` 是写死的字面量。
5. 次要：`ovmf_package` 读固件旁的 changelog，可伪造（放假文件，日志头显示 9999.99-FAKE）；`ovmf_runtime` 的 revision 是通用值 0x00010000，区分不了版本。报告「OVMF 版本实测」有夸大。
6. 次要：build-env.txt 记录了 PATH，换个 shell 冻结的 SHA256SUMS 就对不上。
7. 次要：三个新增文件改为 MIT，但提交信息、报告、BASELINE、规格里都没有 MIT 由谁决定、依据什么的记录；仓库仍无 LICENSE，与 LICENSE_MATRIX「待定」矛盾。门槛 6 无法判断。
8. 次要：冻结仍只靠 chmod，BASELINE.txt 已写明可被属主撤销；SHA256SUMS 自身的哈希只记在冻结目录里的报告副本中。
9. 次要：`bash -x` 跟不进 `env -i` 之后的内层执行；缺工具提示里没写 sha256sum。

### 已核销
旧缺陷 1、3、4、5（清单类）、6（失败日志已补冻，内容是真实失败输出）、7（env.sh 模式已回退）、10（缺工具退出码 3 并有提示）关闭。旧缺陷 2 部分关闭（见本轮 1、2）。

### 修改意见（由总经理转给 codex工具人）
- 去掉用环境变量判断净化的机制，改用 `env -i` 后内层脚本检查自己的环境确实只有允许的变量（`env` 的实际输出与白名单比对，不符就退出），并把内层实测的 `env` 快照写进 build-info，不要写死 `build_env`。
- PATH 里的工具：在 build-info 里同时记录各工具的 sha256 和 `--version`，并检查工具路径落在预期目录；做不到防住，就在文档里写明只做到可见。
- 在 `g6-mmio-map-test.sh` 里补 `mkdir -p`（不改断言）；这是任务第 3 条「复跑问题修复」的同类问题，顺带在干净克隆里用无 build/ 的状态跑一遍全部 UEFI 与 host 测试，找出同类遗漏。
- 报告里 OVMF 一栏改为如实描述（package 来自 changelog，runtime 只是接口修订号）。
- MIT 的决定与依据由总经理补记录或报项目负责人确认。
- 改完递增版本重冻结，我复核差异项并重跑。

### 没核的
盒子上没有第二种 OVMF；HOME 下其他 git 配置对 `-dirty` 判定的影响没穷举；KVM；失败日志只抽查了 7 项。

完整数据：`REVIEW-DATA.md`，日志 `logs/`，哈希 `SHA256SUMS`（本目录）。

---

# 记录 T-M1-0 第 3 轮审查（被审提交 fe3e3cd）

（以下为验收审查原文，只做第二节说明的路径与账号替换，标题层级下移一级）

## T-M1-0 第三版（r3）复核（验收审查）

日期：2026-11-02　送审：work 仓库分支 t-m1-0-r3，提交 fe3e3cd，冻结目录 freeze/afos-m1-t0-0.1.0-dev-fe3e3cd，镜像 sha256 e2837a4f…b933。TCG 下验证，KVM 与其他 OVMF 未测。

### 结论：通过（阶段结论，不带入 T-M1-5），附两条随下一次提交一并修的重要项
通过环境变量把测试宏注入默认镜像的路子已经堵住；复现、启动、冻结、无 build/ 回归都与基线一致。门槛 11 记「部分满足」合适，门槛 6 记「无法判断」符合约定。

### 依据（亲自跑的）
- 两个不同路径的干净克隆（无 build/），按 r3 构建命令构建，镜像都是 e2837a4f…b933，与冻结一致；原地重建、另起 shell 断网构建一致；build-info 与 build-env 与冻结一致，上轮的 PATH 差异已消除。
- 注入实验：先做有效性对照（直接跑 build-bios.sh 时，`NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY=1` 和 CPATH 确实改产物），再走默认构建脚本，你点名的全部变量，加上预设 `AFOS_M1_SANITIZED`、内层用到的变量名、BASH_ENV、导出函数、GIT_DIR、HOME，每一项产物都等于基线；预设 `AFOS_M1_OUT` 退出 2；带脏环境直接调内层退出 4。
- build-info 的环境快照、工具路径、sha256、版本都是内层运行时取的；`build_env` 与 `test_macro_hooks` 已删除。
- 无 build/ 全量回归：同一干净克隆顺序跑，以及每项各用一个干净克隆，两种都是 86 过 9 败，与 round5、旧冻结、r3 冻结逐项相同；在 428d604 上单独跑确认那 4 个测试原来因缺目录失败。
- 启动 3 次串口字节一致且与冻结一致，版本行 build=fe3e3cd，日志头字段齐全。
- 内核源码没变，kernel.elf 与 428d604 只差版本串 7 字节，.text 与 objdump 全文一致。姓名、非 构建用户 的 /home、<工作区>/ 硬编码都是 0 命中。
- 三个冻结目录（f66c5d7、428d604、fe3e3cd）`sha256sum -c` 全过，新目录 626 条，无清单外文件，全部只读，自身哈希 ae09ceb2… 与 FREEZE-HASHES.txt 标的有效项一致。
- 报告措辞：第一条 cli 在 `__kernel_text_start`（0x100000），默认启动结尾是 kernel_main（0x1043ed）的 hlt 循环，ConOut 标为推断，三处与代码一致。

### 关于「防不住，只做到可见」的判断
对工具链目录这一块表述如实：脚本没有固定的预期目录，调用方传什么 `--toolchain-dir` 就认什么，假 nasm 放进工具链目录（含拷贝、符号链接、`..` 伪装）都能改产物且版本行不变，只有 build-info 里的工具路径与 sha256 能看出；版本是 `--version` 自报的，能伪造；sha256 只算 PATH 上那个文件，不覆盖被包装的真实二进制和共享库。够不够由门槛 11 的定义决定：只能证明「同一环境下产物可复现」，不能证明「工具可信」。但报告没写清「预期目录由调用方决定」和 .git 相关的影响，需要补进 README。

### 缺陷清单
1. 重要：`--out=DIR` 在脚本里先 `rm -rf "$out_dir"` 再构建，不做任何检查（build-m1-default.sh:58）。实测删掉了测试目录里原有的文件；照代码 `--out=.` 会删掉仓库根，我没实测这种写法。要求拒绝仓库根、父目录、非空且不含构建标记文件的目录。
2. 重要：README/报告补写「预期目录由调用方决定」「直接调内层时 SYSTEM_DIRS 也能塞假目录」（logs/B-inject2.txt、B-inject3.txt），不要只写假工具「可见」。
3. 次要偏重要：仓库 .git/config 的 `core.fsmonitor` 指定的命令在构建时被执行了 2 次；对 kernel.c 设 assume-unchanged 后改源码，镜像变了而版本行没有 `-dirty`（logs/B-gitconfig.txt）。要求构建时给 git 命令加 `-c core.fsmonitor=false`，并用 `git ls-files -v`/`diff --no-index` 之类不受 assume-unchanged 影响的方式判定脏状态，或把这一项写进限制。
4. 次要：`--toolchain-dir` 末尾带斜杠误报退出 5。
5. 次要：build-env/build-info 随工具链路径写法变化，换工具链路径时冻结清单里这两项会对不上（镜像不受影响）。
6. 次要：FREEZE-HASHES.txt 可写、无校验；作废哈希 10c4c96c… 无法复核（我按「去掉 boot-clean-b 的 3 行」算得 4ca6445a…，对不上）。
7. 次要：测试用的构建脚本仍会吃 NASMENV、CPATH，测试构建未净化，不影响默认镜像。
8. 次要：r2 九项对照表第 1 项「已修复」的说法偏宽，没提直接调内层的情况；`bash -x` 只跟踪外层，`--trace` 才能跟进内层，建议写进 README。

### 没核的
固件与 .deb 的 md5 一致性；替换工具链共享库能否被发现；.git/config 的其他配置项；`--out=.` 的实测；KVM 与其他版本 OVMF。

完整数据：`REVIEW-DATA.md`，日志 `logs/`，哈希 `SHA256SUMS`（本目录）。

---

# 记录 T-M1-0 第 4 轮差异核对与 T-M1-1 阶段验收

（以下为验收审查原文，只做第二节说明的路径与账号替换，标题层级下移一级）

## T-M1-0 r4 差异核对与 T-M1-1 阶段验收（验收审查）

日期：2026-11-02。TCG（QEMU 10.0.13，OVMF 2025.02 Debian）下验证，KVM 与其他 OVMF 未测。阶段结论，不带入 T-M1-5。
数据：本目录 REVIEW-DATA.md（B、C）、<T-M1-0复核目录>/r4/REVIEW-DATA.md（A）；日志在两处 logs/，稳定性统计 logs/stability.tsv、stability-summary.txt；哈希各目录 SHA256SUMS。

### 结论
- A：T-M1-0 r4（cc80bb3）：通过，附一条重要项（未跟踪文件不触发 -dirty）。
- B：T-M1-1（8fb9198）：有条件通过。条件是规格 5a 的方案批准记录补上。
- C：stage7 期望变更（8fb9198）：合理，保留，不回退。

### A 依据（亲自跑的）
- 两个干净克隆、原地重建、断网构建，镜像都是 bb8fb936…2eb6，与冻结一致；kernel.elf 与 fe3e3cd 只差版本串里的构建号，.text 与 .data 相同。
- `--out` 保护：仓库根、`.`、`..`、父目录、`/`、符号链接（带不带斜杠）、`build/..`、空串、普通文件、无标记的非空目录都退出 6，被跟踪文件和哨兵完好；带标记目录、空目录、相对路径能正常构建。m1-build-out-guard-test.sh 通过，含允许路径的对照组。
- `-dirty`：assume-unchanged、skip-worktree、改 initrd 来源、改 tools 脚本、只改 chmod 都出 -dirty；fsmonitor、hooksPath、diff.external、pager、sshCommand、include.path、filter 构建时执行次数为 0，同配置下手动 `git status` 会触发，说明实验有效。
- `--toolchain-dir` 末尾斜杠、`//`、`.`、`..` 规范化成同一路径，镜像一致；直接调内层扩大 SYSTEM_DIRS 退出 2；假 nasm 放进工具目录仍改产物，与 README 的限制说明逐句一致。
- 四个冻结目录校验全过（cc80bb3 为 264 条，无清单外文件，自身哈希与 FREEZE-HASHES.txt 一致）。作废哈希 10c4c96c… 我用 recompute-void-hash.py 复算得到，脚本里该哈希只出现在注释，没有倒推。
- 内核源码未变，现有测试断言未改，作者 afos-codex，姓名、拼音、非 构建用户 的 /home、脚本里 <工作区>/ 硬编码 0 命中。

### A 缺陷
1. 重要：未跟踪文件能改产物而版本行不出 -dirty。实测在 `product/kernel/include/` 放一个未跟踪的 `stddef.h` 就能打开测试宏，镜像变了，版本行仍是 build=cc80bb3（logs/A3-redo.txt，c2 一行）。要求把未跟踪文件纳入 -dirty 判定（至少 product/ 下），或写进限制。
2. 次要：`--out` 允许仓库内的空目录，包括 `.git/refs/tags`，产物写进去后 `git fsck` 报错。
3. 次要：guard 用例没覆盖符号链接、空串、仓库内空目录，也没接进 run-all.sh。
4. 门槛 6：LICENSE 只在规格仓库本地 main 的 599e2da 上，未推送（origin/main 是 494db93），不在被审的 work 分支上。正文与 SPDX 标准 MIT 逐字一致；版权行是 `Copyright (c) 2026 agent-first-os contributors`，不含人名；新文件的 SPDX 行都是 MIT。文件落到仓库前，门槛 6 记「部分满足」。
5. 沿用：冻结只靠 chmod，FREEZE-HASHES.txt 本身没有外部校验。

### B 依据
- 默认镜像 caeeb290…1676、对照镜像 8f7c4a6a…f6d8，两个克隆、原地重建、断网构建都与冻结一致；冻结 287 条与 44 条校验全过。对照组与默认只差 `-DAFOS_M1_PREEMPT=0`，多出的只有抢占相关函数、异常处理的一个分支和 on/off 字符串，用户程序两个节完全相同。设 `AFOS_M1_PREEMPT` 或 `AFOS_M1_VARIANT` 环境变量，构建退出 2。
- 默认启动 5 次、对照 5 次，串口字节都一致且与冻结日志一致；命令行没有 -icount、-rtc。
- 稳定性：默认 TCG 35 次、对照 15 次、4 个与 12 个 yes 负载各 15 次，全部通过，计数恒定：默认 tick=26、tick_cpl3=16、preempt=16、yield=8，对照 34/24/0/8。-icount shift=0、1、2、4 不变；shift=6、8、10（sleep=off）变成 17/27 与 BABA 次序，断言仍通过；shift=12、14 被 QEMU 拒绝。
- 计数为什么恒定：最后一个任务退出后打印第一行计数，idle 收满 10 个 tick 后打印第二行；tick 间隔 54.9ms，任务每轮只要微秒，次序由结构决定，所以宿主变慢几倍结果不变。换 -icount 后数字变了，说明是实测不是写死。这个测试证明 tick 在到达（否则等待超时打印 TIMEOUT）、CPL3 的 tick 引起任务切换；证明不了宿主时间尺度上的抢占频率。
- D1 与 g2-irq：idle 在 IF=0 下判条件后 `sti;hlt`，-d int 里 idle 期间 tick 持续 90 秒没有睡死；所有组次无 TIMEOUT，g2-irq 的 TCG 窗口问题在本任务里消失（g2-irq 用例本身没改，仍失败）。内核阶段只见向量 32 与 0x80，没有 LAPIC 残留向量。
- 断言有效性：对照镜像在 default 模式下判失败；删计数行、preempt 改 0、调高阈值（preempt>=17、idle>=11）、删任务 B 行、run[2]=1、加 EXCEPTION 都判失败。
- 回归（无 build/ 干净克隆）：95 项 86 过 9 败，与 round5、T-M1-0 冻结基线逐项相同，stage7 在新断言下通过。
- syscall 48：无编号冲突，只读。`int $39`：spurious=1，不进异常路径，读 OCW3 的 ISR 位，对伪中断不发 EOI。

### B 缺陷
1. 条件：规格 T-M1-1 第 5a 条要求打印语义方案交总经理，我没找到批准记录，请补记录。
2. 次要：m1-t1-check.sh 在字段取不到时当 0，nopreempt 模式下 preempt 字段缺失照样 PASS（logs/B5-mutation.txt 的 redo）；preempt 只检查最后一行，改第一行不被发现。
3. 次要：向量 40 对照组确实到达 EXCEPTION，但日志里打印 vector=0xff，看不出是 40；真实 IRQ7 发 EOI 的路径和从片伪中断没执行过（logs/B7-spurious-test.txt）。软件 `int $39` 不是真 PIC 伪 IRQ7，接受为限制，建议以后补。
4. 次要：自查表把门槛 9 写成「TCG」，门槛 9 原文是对照组。
5. 次要：非法 syscall 编号与 Ring 3 执行 `int $39` 我只读了代码，没在 Ring 3 实际调用。
6. 未测，接受为已知限制：KVM、其他 OVMF、.deb 签名链、每轮 tick 增量未逐行打印。

### C stage7 期望变更（8fb9198）：合理
旧断言 grep `RING3 ABI READY - launch gated pending TSS`，默认构建现在真的启动 Ring 3，那行不再打印。四格我自己跑：fe3e3cd 旧行 1 次、新断言 0 行；8fb9198 旧行 0 次、新断言 2 行；两边 stage7 都 rc=0。run[] 只在任务切换成功和首次启动时增加，idle 和初始化不会增加。第二证据来自 -d int：16 个 CPL3 的向量 32 在两个任务地址（0x400298 与 0x187298）之间交替，两个地址段都有大量 int 0x80。

### 没核的
KVM；其他 OVMF；真实 IRQ7 与从片伪中断；`--out=` 指向的其他仓库内目录类型的全覆盖；-d int 原始追踪约 160MB 留在 <临时目录>/r4/，没入库。

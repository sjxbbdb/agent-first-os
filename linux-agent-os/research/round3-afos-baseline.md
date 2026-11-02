# 自研内核基线调研：本仓库现状、第一里程碑差距与 Linux 规格复用

日期：2026-11-02　性质：只读调研（未编译、未运行 QEMU、未修改任何仓库）

## 一、读到的版本

- 仓库：https://github.com/sjxbbdb/agent-first-os ，只读克隆于 `<只读克隆目录>`
- `git log -1 --format='%H %cI'` 输出：`ad0fd51618da5a020ece90871a44e568ad746e01 2026-09-16T15:48:33+08:00`（即 2026-09-16 15:48 北京时间）
- `product/VERSION`：`0.1.0-dev`
- 本地工作副本 `<本地副本>` 未改动；其 `linux-agent-os/` 只读取。`git status --short` 显示 `?? linux-agent-os/prototypes/`（未跟踪目录，本次未读取、未改动）。

## 二、仓库现状

### 2.1 目录结构（`find . -type d`）

```
learning/  curriculum(archive) evidence lessons notes(lesson-log) projects(stage-0/day-02-compile) source-track
product/
  kernel/  src/ include/ arch/x86_64/{boot/bios, boot/uefi}  linker.ld user_linker.ld
  userland/include/        仅 4 个协议头文件
  services/ (+examples/)   JSON schema、协议文档、宿主侧 JS/Python 参考实现
  agent-runtime/{protocol,reference}  JSON schema + 宿主侧 JS 参考实现
  tools/   build-bios.sh build-bios.ps1 build-uefi.sh run-bios.sh make-initrd.py
  tests/   约 140 个文件（*.sh / *.c host 测试 / *.test.js）
  docs/    DESIGN_BOOK.md OVERALL_FRAMEWORK_PLAN.md IMPLEMENTATION_PLAN.md G*_*.md decisions/ assets/
```
- 没有 Makefile（`find product -name 'Makefile*'` 无输出）；构建全靠 `product/tools/*.sh`。
- 仓库没有根级 `tests/` 目录（`ls tests` 报 No such file），测试在 `product/tests/`。
- `product/docs/decisions/` 只有一份：`DECISION-0001-agent-runtime-harness.md`。
- `learning/` 有 `evidence/` 下约 90 份 M*/G* 证据文件（串口日志摘录与说明）。

### 2.2 各模块作用（依据文件名与头文件/源码注释）

| 文件 | 行数 | 作用 |
|---|---|---|
| kernel/src/kernel.c | 1830 | 主体：GDT/TSS/IDT、串口输出、syscall 分发（`kernel_syscall_handler` 550 行起）、异常处理、调度入口、`kernel_main`（1392 行起） |
| kernel/src/process.c | 500 | 进程表（`AGENT_OS_MAX_PROCESSES 32u`，include/process.h:12）、轮转调度 `agent_os_process_schedule`（process.c:184）、wait/kill/restart、任务组 |
| kernel/src/capability_table.c | 292 | capability 表：rights、generation、派生 lineage、撤销 |
| kernel/src/policy_token.c | 110 | 一次性 policy token：32 槽、绑定 owner/capability/action_nonce/expires_at/generation（include/policy_token.h:9-21） |
| kernel/src/ipc_endpoint.c / shared_memory.c | 109 / 190 | 同步 endpoint、共享内存 |
| kernel/src/vm.c / address_space.c / physmem.c | 406 / 262 / 77 | 4 KiB 页表、每进程 CR3、E820 物理页分配 |
| kernel/src/elf_user_loader.c / initrd.c | 131 / 57 | ELF64 PT_LOAD 用户程序装载、固定格式 initrd |
| kernel/src/virtio_pci.c | 586 | virtio-blk/net/input legacy 传输 |
| arch/x86_64/boot/uefi/uefi_loader.c | 641 | 自写 PE/COFF UEFI loader（`efi_main` 539 行），读 ESP 上 KERNEL.ELF，ExitBootServices 后进入 kernel_entry |
| arch/x86_64/user_program.asm | 2611 | **全部 Ring 3 程序**：Supervisor/Policy/Agent/服务都是这里按 `%ifdef AGENT_OS_TEST_*` 选择的汇编 fixture |
| services/*.js, *.py | — | 宿主侧（Node/Python）参考实现：supervisor_runtime.py、policy_registry_runtime.js 等，不在内核里运行 |
| agent-runtime/reference/*.js | — | 宿主侧 Agent Runtime 参考（runtime.js、pi_runtime.js） |

### 2.3 代码行数（未安装 cloc：`which cloc` 无输出，故用 wc）

```
$ find product -name '*.c' -o -name '*.h' -o -name '*.S' -o -name '*.asm' | xargs wc -l | sort -n | tail -1
 10962 total            # 含 product/tests 下的 host C 测试
$ cat $(find product/kernel -name '*.c' -o -name '*.h') | wc -l
6270                    # 内核 + UEFI loader 的 C/头文件
$ cat $(find product/kernel -name '*.asm') | wc -l
3538                    # 汇编（含 BIOS stage1/2、入口、中断、user_program.asm 2611 行）
```

### 2.4 已实现功能（代码层面确认存在；运行效果未验证）

syscall 清单见 `product/kernel/include/syscall.h:10-66`（int 0x80）：EXIT/WRITE/YIELD/WAIT/KILL/RESTART、GROUP_TERMINATE/FREEZE/RESUME/TERMINATE_TREE、IPC_CALL/RECV/REPLY/RECV_WAIT/CANCEL/CLOSE/CREATE/DESTROY、CAP_RESTRICT/TRANSFER/REVOKE、SHM_CREATE/MAP/UNMAP、POLICY_TOKEN_MINT/CONSUME/REVOKE、POLICY_PAUSE/RESUME、VIRTIO_BLOCK_WRITE/FLUSH/READ。

- 串口输出：`kernel.c:408` `outb(0x3F8, ...)`，`serial_print` 声明于 kernel.c:72。
- 调度：协作式轮转（process.c:184-210），任务退出切换打印 `SCHEDULER switched task`（kernel.c:1349）；PIT 抢占仅在 `AGENT_OS_TEST_TIMER_PREEMPT` 下编译（kernel.c:239、391-401）。
- 关键事实：内核里 `AGENT_OS_TEST_` 宏出现 38 处（`grep -c AGENT_OS_TEST_ kernel.c` = 38），全仓内核共 46 个不同的测试宏（`grep -o ... | sort -u | wc -l` = 46）。大部分功能是**按测试宏编译的纵切 fixture**，没有一个“默认构建即具备全部能力”的内核镜像。

### 2.5 构建与 QEMU 运行方式（只记录，未执行）

- 内核（BIOS/ELF）：`product/tools/build-bios.sh`，gcc `-ffreestanding -fno-pie ...`（行 43-76）+ nasm `-f elf64`（行 79-81）；通过 `KERNEL_CFLAGS_EXTRA`、`USER_NASMFLAGS_EXTRA` 注入测试宏；产物 `build/bios/kernel.elf`。
- UEFI：`product/tools/build-uefi.sh`，`clang --target=x86_64-pc-windows-msvc` + `lld-link /subsystem:efi_application /entry:efi_main`（行 15-22），默认源是 `uefi_stage0.c`（行 7），完整 loader 需 `UEFI_LOADER_SOURCE=.../uefi_loader.c`；`UEFI_KERNEL_ELF`、`UEFI_INITRD_BIN` 拷到 ESP 的 `AGENTOS/`。
- 典型 UEFI 运行（`product/tests/g5-uefi-kernel-test.sh:10-24`）：
```
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 bash product/tools/build-bios.sh
UEFI_BUILD_DIR=build/uefi-full UEFI_LOADER_SOURCE=product/kernel/arch/x86_64/boot/uefi/uefi_loader.c \
UEFI_KERNEL_ELF=build/bios/kernel.elf bash product/tools/build-uefi.sh
cp /usr/share/OVMF/OVMF_VARS_4M.fd build/uefi-full/OVMF_VARS.fd
product/tests/qemu-timeout.sh 12s <log> -machine q35 -m 128M -nodefaults -device VGA \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=build/uefi-full/OVMF_VARS.fd \
  -drive format=raw,file=fat:rw:<esp_dir> -serial stdio -display none -no-reboot
```
  `qemu-timeout.sh` 把 `-serial stdio` 改写成 `file:<log>`，超时退出码 124 视为通过（g5 测试行 28）。
- 全量回归：`product/tests/run-all.sh`（串行，BIOS 构建目录共享，行 20-21 注释）。README.md:39-43 给出的是在 Windows WSL2 下调用的命令。
- 依赖工具（从脚本推出）：gcc、nasm、clang、lld-link、qemu-system-x86_64、OVMF 4M 固件、node、python3。

## 三、文档与代码的出入

| # | 文档声称 | 代码实际 | 依据 |
|---|---|---|---|
| 1 | README：`userland/` 含“原生用户态运行时、libc、shell 和系统工具” | 只有 4 个 `.h` 协议头文件，无 libc、无 shell | README.md:28；`ls product/userland/include` |
| 2 | README：`services/` 含 Supervisor、文件、进程、网络、窗口、输入模块 | 原生侧没有独立服务程序；services/ 是 schema + 宿主 JS/Python；原生 Supervisor/Policy/Agent 是 user_program.asm 里的汇编 fixture | README.md:29；`ls product/services`；user_program.asm |
| 3 | DESIGN_BOOK：Supervisor 是“第一个用户态进程”并拉起策略层、服务、agent | 只有测试宏 `AGENT_OS_TEST_SUPERVISOR*` 下的 fixture，“current boot slice intentionally selects service 0” | DESIGN_BOOK.md:116；kernel.c:198-200、1731、1817 |
| 4 | DESIGN_BOOK：Trusted Input 是“agent 无法模拟的可信输入通道” | 只有 IPC 协议常量与 fixture nonce 往返（0x83/0x93），无真实输入设备隔离、无签名 | DESIGN_BOOK.md:174、318；userland/include/semantic_registry_protocol.h:10,16；user_program.asm:1155,1194；OVERALL_FRAMEWORK_PLAN.md:19 自认“不扩大为生产 Registry 或真实输入实现” |
| 5 | DESIGN_BOOK：token “绑定任务、对象、动作、范围和过期时间”（README.md:191） | token 结构只有 owner_process、capability、action_nonce、expires_at、generation；无“范围”字段，无密码学签名（是内核表句柄） | include/policy_token.h:13-21 |
| 6 | DESIGN_BOOK：本地操作“默认写入事务日志，支持恢复到任务前状态” | 原生 journal 仅在 `AGENT_OS_TEST_G7_NATIVE_JOURNAL_*` 下、固定内存/单扇区 fixture；持久化 journal 只有宿主 JS（DurableJSONLJournal） | DESIGN_BOOK.md:46；kernel.c:1673-1693；OVERALL_FRAMEWORK_PLAN.md:19、21 |
| 7 | DESIGN_BOOK：capability 绑定“任务 ID、作用范围和有效期”（README.md:210） | capability 表有 rights/generation/lineage，未看到任务 ID/有效期字段 | include/capability_table.h:19 附近（未逐字段核对全部，部分未验证） |
| 8 | DECISION-0001：Agent Runtime 基于 pi agent | 原生侧无 pi agent；仅宿主 JS 参考与 mock model | decisions/DECISION-0001:7,26；OVERALL_FRAMEWORK_PLAN.md:19 |
| 9 | build-uefi.sh 默认就是“UEFI 产品启动器” | 默认源是 `uefi_stage0.c`，完整 loader 需显式覆盖 | build-uefi.sh:7 |
| 10 | 文档无资源配额说明，草案需要“结构化接口申请资源” | 代码无 quota/limit/budget 语义（`grep -i 'quota\|limit\|budget'` 只命中 GDT/IDT limit 与地址上限） | grep 输出 |

说明：仓库自己的 OVERALL_FRAMEWORK_PLAN.md:9-21 对上述多数限制有如实标注（“bounded slice”“仍未完成”），出入主要在 README/DESIGN_BOOK 的模块描述与代码之间。

## 四、对第一里程碑草案的差距表

草案：UEFI 启动进内核 → 串口输出 → 创建并调度两个任务 → 任务通过结构化接口申请资源 → 一条需人工确认才放行的外部动作；不碰驱动和网络栈。

| 草案项 | 已有（代码） | 缺失 | 文档有代码无 |
|---|---|---|---|
| UEFI 启动进内核 | uefi_loader.c（efi_main 539、ExitBootServices 60/91/269）；g5-uefi-kernel-test.sh 期望 `boot_info.loader_type=0x...02` | 默认构建脚本源是 stage0（build-uefi.sh:7） | GOP/ACPI 完整降级矩阵（OVERALL_FRAMEWORK_PLAN.md:18 自认未完成） |
| 串口输出 | kernel.c:408 COM1 | 无结构化日志格式（无 JSON/行协议规定） | — |
| 创建并调度两个任务 | 进程表 32 槽、每进程 CR3、轮转调度、ELF 装载、exit/wait/kill（process.c:184；kernel.c:1349） | Ring 3 无通用 spawn syscall（SYS_RESTART 仅重启父拥有的 zombie，syscall.h:16-19）；任务来自内嵌 ELF/initrd；默认不抢占（PIT 仅测试宏） | DESIGN_BOOK 的“任务进程组由 Supervisor 建立”（DESIGN_BOOK.md:209）仅 fixture |
| 结构化接口申请资源 | capability restrict/transfer/revoke、IPC、SHM syscall（syscall.h:25-46） | 无“资源申请”语义：无配额、无请求/批准的数据结构、无 schema 化 syscall 参数（int 0x80 裸寄存器） | DESIGN_BOOK 的 Semantic Registry/Tool schema（DESIGN_BOOK.md:121、316）原生无实现 |
| 需人工确认才放行的外部动作 | 内核 policy token mint/consume/revoke + pause/resume（policy_token.c；kernel.c:1048、1089）；G10 链 marker `SYSCALL policy token mint OK`/`consume denied`（g10-uefi-chain-test.sh:23-27） | 确认来源是 fixture，不是人；无 Webhook 渠道、无签名 token、无“待确认-冻结-超时拒绝”状态机；“外部动作”目前是 virtio-block 写 / mock 服务 | Trusted Input（DESIGN_BOOK.md:174、318） |
| journal 回滚 | 原生 fixture：`JOURNAL PREPARE/COMMIT/REPLAY_REJECTED/ROLLBACK`（g10-uefi-chain-test.sh:23-25） | 非持久、非通用 | DESIGN_BOOK.md:46、222 的任务日志与断点恢复 |
| 不碰驱动/网络栈 | — | 内核含 virtio_pci.c 586 行，里程碑需裁剪或不编译 | — |

**最大差距 3 条**
1. **没有真实人工确认通道**：确认是汇编 fixture 自给自足，缺 Webhook + 签名 token、待确认冻结与超时拒绝（依据：user_program.asm:1155,1194；policy_token.h:13-21）。
2. **没有“结构化资源申请”语义**：syscall 是裸寄存器 ABI，无配额/请求对象/schema（syscall.h:7-9；grep quota 无结果）。
3. **能力按测试宏分散**：46 个测试宏各自产出不同镜像，里程碑需要一个默认构建、单一镜像同时具备 UEFI+两任务+申请+确认（kernel.c 38 处宏）。

## 五、Linux 规格的保留与搁置清单

出处根目录：`<本地副本>/linux-agent-os/`

### 5.1 可直接保留（与 Linux 无关）

| # | 内容 | 出处 | 自研内核需求表述 |
|---|---|---|---|
| K1 | 外部副作用一律人工确认，回滚只覆盖本地状态 | decisions/0001:6,10；research/round1:31-33 | 任何离开 QEMU 虚拟机的动作（网络、消息、外部 API）在内核侧标为不可逆，必须先消费一个经人工确认的一次性 token 才放行；日志/回滚不得把它标成可撤销 |
| K2 | 确认渠道先用 Webhook + 签名 token | decisions/0002:7；research/round2:16-17,78 | 确认服务把待确认动作经 Webhook 推出；所有者批准后返回签名 token（绑定任务、动作摘要、风险级、过期时间、nonce）；内核或策略服务验签后才放行，重放拒绝 |
| K3 | 待确认时任务冻结、超时默认拒绝 | research/round2:17,78 | 申请 L2/L3 动作的任务进入 awaiting_confirmation，调度器不运行它；超时转 cancelled（与 DESIGN_BOOK.md:194-207 状态机一致） |
| K4 | 风险分级 L2/L3 | research/round2:80（引 DESIGN_BOOK 171-172） | 每个外部/高危动作在请求结构里带风险级；L2 需确认，L3 强制确认或维护模式 |
| K5 | 确认 token 由谁验证（待拍板） | decisions/0002:16；specs/proto3:21 | 自研内核可按设计书由内核验证；仍列为「待拍板」，不得当已定 |
| K6 | 开发阶段保留 shell，稳定后去掉改带外维护 | decisions/0002:6；specs/proto3:13,32-33,63-66 | 内核提供 dev/stable 两种构建配置；dev 配置串口可交互调试，stable 配置无 shell；stable 的判据（镜像清单无 shell + 串口无响应两条证据）可沿用 |
| K7 | 策略与执行权分离 | research/round2:66 | 能签发/改策略的任务不能创建任务，能创建任务的不能改策略；用 capability 区分 |
| K8 | 任务资源限制语义（pids 上限、pause/resume/kill 整组） | specs/proto3:44-49,60-63 | 任务创建请求带结构化 limits（至少最大子任务数，建议加内存页数、CPU 时间片）；整组冻结/恢复/终止；状态可只读查询（存活、冻结）。示意 JSON 形态 `{"op":"spawn",...,"limits":{"pids":8}}` 可作为接口原型 |
| K9 | 快照/回滚语义收窄为「文件系统 + 任务元数据」，按步骤打检查点，不重放不可逆外部动作 | research/round1:72-77；round2:19-20,76 | journal 以“任务-步骤”为粒度记录前像、动作 ID、token、结果、可撤销性；回滚只针对本地对象；不可逆动作禁止自动重放 |
| K10 | 审计 append-only、每条含 task/hook/verdict | research/round2:77,89 | 内核/策略决策写结构化 append-only 记录（串口行或日志对象），每条含任务、动作、判定；后续可外送防篡改 |
| K11 | 送审材料清单（缺一项即不通过） | specs/proto1:72-82；specs/proto3:86-96 | 去掉“内核 .config / GPL busybox”项后保留：提交位置、完整构建命令与日志、QEMU 命令行、串口全量日志落文件、宿主请求/响应记录、用例源码与输出、许可证说明、来源链接；别人可原样复跑 |
| K12 | 串口日志与 QEMU 证据要求、对照组 | specs/proto1:51-64；tasks/proto1-landlock-tasks.md:9,13 | 每条用例给期望/实际；有不加限制的对照组证明拒绝来自机制本身；不符如实报告，不得改期望 |
| K13 | 只在 QEMU 验证，不碰真机 | specs/proto1:26；specs/proto3:35 | 与项目负责人原则一致：真机后期且必须先问 |
| K14 | 任务分工与验收流程（规格→实施→验收审查复跑） | tasks/proto1-landlock-tasks.md:6-18 | 模板可直接复用，任务内容需重写 |
| K15 | agent 框架本身只是一个受限任务 | research/round2:66 | Agent Runtime 作为普通 Ring 3 任务运行，受同样 capability/limits 约束（与 DECISION-0001 不变量一致） |
| K16 | agent 间通信：带身份、带权限、可审计的消息 | research/round1:52-53 | 本仓库已有 capability endpoint，需求可直接映射为“IPC 消息携带发送者身份、按 capability 放行并记审计” |

### 5.2 Linux 专属，搁置

| 内容 | 出处 |
|---|---|
| Landlock（规则集、ABI 查询、`landlock: Up and running.` 判据） | specs/proto1 全文；specs/proto3:30,54-55 |
| cgroup v2（cgroup.freeze/kill、pids 控制器、cgroup.events） | specs/proto3:46-48,60-63；tasks T4 |
| BPF LSM 审计/放行 | research/round1:32,97；round2:60,77,87 |
| `make tinyconfig`、6.18 longterm 基线、.config 差异 | specs/proto3:30,89；specs/proto1:16,75 |
| namespace、SO_PEERCRED/pidfd、unix socket 路径（/run/agent/*.sock） | research/round1:53,96；round2:63,70-78 |
| overlayfs / btrfs / FICLONE 快照、CRIU | research/round1:30-32,59,73-75 |
| busybox initramfs、GPL busybox 义务 | specs/proto1:79；specs/proto3:31-33,93 |
| gVisor、Firecracker/microVM（阶段 E） | decisions/0002:5；research/round2:18-19 |
| io_uring 取舍 | research/round1:90 |
| `-kernel/-initrd/-append lsm=` QEMU 参数（自研走 OVMF+ESP） | specs/proto1:77 |
| 注：decisions/0001:5「只做 Linux 魔改，自研内核暂不动」已被本次项目负责人决定推翻，需新增决定记录说明 | decisions/0001:5 |

## 六、卡点和未验证项

- **未编译、未运行**：所有“功能存在”结论仅基于源码与测试脚本；串口 marker 是否真实出现未验证（按分工交由 codex工具人复跑 `run-all.sh` 或 g5/g10-uefi 测试）。
- cloc 未安装，行数用 wc（含注释与空行）。
- capability 表是否含任务 ID/有效期字段：只看了 include/capability_table.h:19 一处，完整字段未核对，未验证。
- `learning/` 下约 90 份证据文件只列了文件名，内容未读，未验证与代码一致。
- `product/docs/IMPLEMENTATION_PLAN.md`、`GATE_AUDIT_2026-09-14.md`、`G7_*.md`、`agent-runtime/` JS 实现未细读，未验证。
- `<本地副本>/linux-agent-os/prototypes/` 为未跟踪目录，本次未读。
- README.md:42 的 WSL2 命令在 Linux 盒子上的等价调用方式未验证（脚本均为 bash，预计直接 `bash product/tests/run-all.sh`，未验证）。
- 待拍板：确认 token 由内核还是策略服务验证；资源 limits 的具体字段；“稳定”判据；里程碑是否复用本仓库代码（fork）还是只复用设计。

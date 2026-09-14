# Agent-First OS 整体框架长链路计划

日期：2026-09-13

这份计划定义“整体框架搭建完成”的边界：在 QEMU/OVMF 与 SeaBIOS 上，系统能从统一的启动交接协议进入一个具备真实地址空间隔离、最小调度、IPC/capability、Supervisor、基础用户态服务、策略事务和 Agent Runtime 适配接口的可运行纵向闭环。它不把 QEMU 验证扩大成真实 PC、完整桌面或生产级模型服务。

## 当前审计结论

初始 M0–M7 审计发现过共享 2 MiB 页表、BIOS GDT 指针和停机式 `SYS_EXIT`。当前基线已经在 BIOS/QEMU 与 OVMF 的 bounded slice 中接入 BootInfo v2、kernel-owned GDT/TSS、4 KiB 页表、NX/W^X、per-process CR3、用户 ELF、调度退出、IPC/capability、Supervisor、UEFI、virtio-block、virtio-net TX、Policy/Registry/Agent 原生链；主机侧和 QEMU 证据仍按各自范围标注，不能替代尚未完成的通用装载、输入/RX、持久化事务、真实模型和完整故障矩阵。

因此，后续工作不能从“接入 agent”开始，必须先完成地址空间和对象生命周期。每个 gate 都要有实际启动或故障注入证据，编译成功不算通过。

## 执行状态

- **G0：BIOS/协议基线通过。** `abi.h`、`boot_info_v2.h`、`ipc.h`、`capability.h`、`syscall.h` 和 Agent Runtime 的 ActionPlan/envelope schema 已加入；BIOS 已写入 BootInfo v2；`product/tests/g0-contract-test.sh` 与 PowerShell BIOS build 已在 WSL2 通过；版本与来源清单见 `product/VERSION`、`LICENSE_MATRIX.md`。UEFI 适配仍属于 G5。
- **G2：单核地址空间、嵌入式用户 ELF、cooperative kernel slice 与用户 fault recovery 通过。** `process.c`、kernel-owned address-space builder 和受校验的 ELF64 `PT_LOAD` loader 已接入 BIOS kernel；第一个 Ring 3 fixture 从独立构建的 `user.elf` 复制并映射到独立 CR3，两个 fixture 在启动和 task 1→task 2 frame replacement 时真实加载 CR3，再到 idle；遗留 PIC IRQ0 也有单独长循环回归，当前通过启动前屏蔽 PIC fail-closed。另有 generation/wait/reap 主机测试、child `kill`→zombie→`wait`→`reap` QEMU 证据，以及 task 1 用户 #PF→zombie→task 2 继续运行的 QEMU 证据，见 `learning/evidence/g2-fault-recovery.md`。新增 bounded 多区域 E820 物理页分配器，能跳过 bootstrap、kernel、staging、initrd 和 E820 表保留范围并跨区域分配；BIOS 路径已将其有界接入页表页分配，PMM 不可用时回退 bootstrap pool，证据见 `learning/evidence/g2-physical-allocator.md` 与 `g2-physical-allocator-vm.md`。动态文件服务装载、PIE/ET_DYN、多段用户 ELF 的通用物理装载、IRQ remap 与单核 PIT timer preemption 已有定向证据，阻塞唤醒和通用 fault policy 仍待扩展；当前只新增了经过校验的固定 initrd 多服务选择器。
- **G3：内核 IPC 与 capability 首个垂直切片通过，多等待者、撤销链、取消和动态 endpoint 生命周期已有证据。** bounded capability/IPC/shared-memory implementation 已加入 generation/revoke、权限边界、队列和页对齐/溢出负向测试；派生 capability 保存父表 incarnation/句柄，祖先撤销会使跨表后代失效，分支撤销不影响 sibling，测试见 `product/tests/g3-cap-lineage-test.sh`。BIOS/QEMU Ring 3 fixture 已真实完成 `SYS_IPC_CALL` → capability lookup → endpoint enqueue → `SYS_IPC_RECV` → 用户缓冲区 copy-out，并让两个独立 CR3 映射同一 capability-authorized shared page；`SYS_CAP_RESTRICT`/`SYS_CAP_TRANSFER`/`SYS_CAP_REVOKE` 也有 QEMU 证据，且 transfer 已让 task 2 使用新句柄接收消息。`SYS_IPC_RECV_WAIT` 允许多个接收者按 FIFO 跨 CR3 投递，父进程可取消阻塞子任务并 wait/reap，`SYS_IPC_CLOSE` 还证明关闭 endpoint 后旧 capability 发送被拒绝；空的 closed endpoint 也会立即返回而不会重新登记 waiter。新增的 Ring-0 四槽 endpoint pool 由 `SYS_IPC_CREATE/DESTROY` 管理，动态 capability 可真实收发、销毁后 generation 失效，关闭/销毁会唤醒等待者，并在 endpoint 销毁时撤销其他进程中转移出的同对象句柄；证据见 `learning/evidence/g3-kernel-blocking-ipc.md`、`learning/evidence/g3-kernel-multiwait-ipc.md`、`learning/evidence/g3-kernel-ipc-cancel.md`、`learning/evidence/g3-native-ipc-close.md`、`learning/evidence/g3-dynamic-endpoint.md` 与 `learning/evidence/g3-endpoint-destroy-lineage.md`。超时、reply 对象、命名/服务发现和一般对象分配仍待扩展。
- **G5/G6：UEFI 内核交接纵切通过，virtio 传输纵切进行中。** G5 已由自写 clang/lld-link PE/COFF loader 从 FAT ESP 读取并校验同一 `KERNEL.ELF`，构造 BootInfo v2、转换 UEFI memory map、收集 GOP/ACPI 提示、调用 `ExitBootServices`，再由 OVMF 进入同一个 `kernel_entry` 并运行 Ring 3；坏 ELF、注入 stale map-key 的重试和固定格式 initrd 交接已有 OVMF 证据，通用重定位和完整负向矩阵仍未完成。G6 已完成 virtio-blk sector-0 读取、virtio-net 初始化与真实 legacy queue 1 TX 完成、virtio-input 发现和一个受门控的 legacy queue arm/poll 实现（当前 QEMU BIOS 路径未暴露可用 legacy BAR，专用测试明确 BLOCKED），以及真实 BIOS/QEMU Ring 3 file request/response IPC、固定内存 file-write/replay fixture、network/input service IPC interface 和 synthetic input/window IPC 边界测试；TX 证据见 `learning/evidence/g6-virtio-net-tx.md`，输入窗口协议证据见 `learning/evidence/g6-native-input-window.md`。这些协议切片仍不等同于 RX packet、virtio-input event queue、独立窗口服务或持久化文件系统；完整故障矩阵仍未完成。
- **G4/G7/G8/G9：原生 Supervisor 与 Policy/Agent 纵切进行中。** G4 已有 version 2 多服务 initrd 容器：BIOS 与 UEFI/OVMF 都能验证 manifest、service table 和有界 ELF image，并由 Ring 3 Supervisor fixture 启动第一个服务；证据见 `learning/evidence/g4-native-supervisor.md`、`learning/evidence/g4-uefi-initrd.md`、`learning/evidence/g4-multi-initrd.md`。新增的 `g4-supervisor-fault-restart-test.sh` 还证明服务真实用户 #PF→zombie、父进程限定 `SYS_RESTART`、第二 heartbeat 和 `SYS_WAIT` 回收，证据见 `learning/evidence/g4-supervisor-fault-restart.md`。Supervisor host reference 另有依赖 READY 门、task-group freeze 和 generation capability 回收证据，见 `learning/evidence/g4-supervisor-groups.md`；原生内核现在有父级限定、非递归 direct-child group terminate 与 wait/reap 证据，见 `learning/evidence/g4-native-task-group.md`，并已有父级限定 direct-child freeze/resume 的 BIOS/QEMU 证据，见 `learning/evidence/g4-group-freeze.md`。新增 destructive `kill` 路径的 capability 自动回收和 generation 失效 host 证据见 `learning/evidence/g4-capability-reclaim.md`；clean exit 的 restart-compatible 回收仍待扩展；递归 terminate 的 host bounded evidence 见 `learning/evidence/g4-recursive-task-group.md`，原生三层 test-gated bootstrap 已通过 `learning/evidence/g4-native-recursive-task-group.md`；general process creation 和 recursive freeze/resume 仍待扩展。G7 新增内核 generation-tagged policy token、expiry 和 emergency pause/resume：Ring 3 policy authority 绑定 capability/action nonce，暂停时 fail closed，恢复后可消费，过期 token 被拒绝；证据见 `learning/evidence/g7-kernel-token.md`、`learning/evidence/g7-kernel-pause.md`。 Host reference 另新增 `DurableJSONLJournal`，以追加、`fsync`、重启恢复和截断尾 fail-closed 证明策略日志接口边界，证据见 `learning/evidence/g7-durable-journal.md`；它不构成原生持久化事务或真实磁盘保证。G8 新增 BIOS/QEMU Ring 3 Semantic Registry/Context/Trusted Input 协议纵切：未知工具被拒绝，版本化工具元数据、任务窗口摘要、action digest 和 Trusted Input nonce 通过 bounded IPC 返回并由 Agent 校验；证据见 `learning/evidence/g8-native-semantic.md`，明确不把这些 fixture 扩大为生产 Registry 或真实输入实现。G9 另有 BIOS/QEMU Agent→Policy IPC 纵切，并在 UEFI/OVMF 上复跑：Agent 发送 action nonce，Policy 用 admin capability 签发绑定 token，Agent 消费后才提交；`PiAgentLoop` 已提供 dependency-free 的 structured response、approval、checkpoint/recovery、heartbeat 和 backpressure reference；新增 host-side `DurableCheckpointStore`、JSON-over-HTTP remote adapter 与 bounded disconnect retry，分别覆盖 fsync/reopen checkpoint、超时/错误回退/ActionPlan 绑定校验和一次断线重试，证据见 `learning/evidence/g9-native-agent.md`、`learning/evidence/g10-uefi-agent.md`、`learning/evidence/g9-pi-runtime.md`、`learning/evidence/g9-durable-checkpoint.md`、`learning/evidence/g9-http-adapter.md`、`learning/evidence/g10-disconnect-recovery.md`。实际 pi/remote model runtime、原生完整 Registry/Agent Runtime 服务和完整 QEMU 端到端闭环仍未完成。
- **G1：bootstrap 与高半别名准备通过。** Kernel-owned GDT/TSS、BootInfo v2、4 KiB bootstrap PT、NX/W^X 叶权限和 page-table walk 已有 QEMU 正/负证据；每个进程 root 还为 kernel text/rodata/data/bss 建立了同物理页、同 W^X 权限的高半别名，并由 host 与 BIOS/QEMU 检查 low-exec 保留，证据见 `learning/evidence/g1-high-half-alias.md`。已通过一个切换到进程 CR3 的无状态高半 RIP probe，并验证低地址栈回退；高半栈、C 函数/全局数据、通用 ELF 地址空间和完整 relocation 仍按 G2 后续扩展。`g1-permission-test.sh` 与 `g1-high-half-test.sh` 纳入串行回归。
- **G6/G10：仍未封门，但已有受限原生证据。** G6 已有 QEMU virtio-blk PCI legacy transport 的真实探测、特性协商、queue 0 初始化和 sector-0 读取证据，并有 virtio-net 初始化与 virtio-input 设备发现；host-only fixture 已覆盖文件/网络/输入/窗口服务协议的边界与能力失败语义，BIOS/QEMU 另有固定内存 file-write、一次性 token、单次写入、replay 拒绝和 synthetic input/window IPC 边界证据，但 virtio-net RX 与 virtio-input event queue 仍未完成，且没有持久化文件系统。G10 现在在 BIOS 与 OVMF/UEFI 都有同一固定 mock-model→Registry marker→Policy token→Ring 3 service→postcondition→journal commit/replay rejection，以及故意后置条件失败后恢复内存前像的 `JOURNAL ROLLBACK` 原生组合链；证据见 `learning/evidence/g10-native-chain.md`、`learning/evidence/g10-uefi-chain.md`。Host HTTP adapter 另有 bounded disconnect recovery，但这仍未接入远程模型、生产 Registry/真实服务，也未完成提示注入、断网的原生路径、崩溃、暂停和完整持久化回滚故障矩阵。代码会按依赖顺序逐 gate 合入；任何只读接口或 host-side stub 都不会被计为 QEMU/OVMF 完成证据。

## 目标闭环

```text
SeaBIOS / UEFI
    → ELF64 + BootInfo v2
    → kernel_entry / 内核初始化
    → 隔离的进程地址空间与调度
    → IPC + shared memory + capability
    → Supervisor 启动服务
    → Policy Firewall / 事务日志
    → Semantic Registry
    → Ring 3 Agent Runtime（pi 主线，Codex harness 适配参考）
    → 结构化动作 → capability/syscall/IPC → 结果验证与恢复
```

## Gates

### G0：契约冻结与基线重构

建立统一错误码、版本化 syscall/IPC 头、BootInfo v2 草案、共享构建脚本、`run-all` 证据入口和许可证清单。保留 BIOS 教学路径，但把 BIOS 专用地址、GDT 和临时字段从公共内核接口中隔离出来。

验收：干净工作树可从 PowerShell 构建；所有 ABI 有定宽字段、大小和偏移断言；每个测试保存命令、版本、串口日志和负向结果。

### G1：内核基础硬化

把 GDT、TSS、IDT 和 bootstrap stack 的所有权迁入内核架构模块；BootInfo v2 明确 loader 类型、内存图和可选平台信息，不再依赖 `reserved` 传 BIOS GDT。把当前共享 2 MiB 大页替换为 4 KiB 页表构建器，至少支持内核 RX/RW、用户 RX/RW/NX 和不可访问页。

验收：内核和用户不能互相写入；故意访问未映射页进入可解释的 page fault；页表破坏测试 fail closed；BIOS 仍可进入同一 `kernel_entry`。

### G2：进程、地址空间与单核调度

引入 `Task/Process/Thread/AddressSpace` 对象、每进程 CR3、静态 ELF 用户装载、用户栈、`exit/wait/kill` 和最小 cooperative scheduler，再加入 PIT/APIC timer 作为可选抢占入口。当前 fixture 已完成每进程 CR3、嵌入式 ELF `PT_LOAD` 装载、用户栈和 `exit/wait/kill` 的首个垂直切片；下一步是动态多段装载、timer、阻塞唤醒和 fault-to-task recovery。`SYS_EXIT` 改为线程/进程退出并回收，而不是停住整台 CPU。

验收：两个隔离 Ring 3 进程能分别运行、退出和回收；一个进程崩溃不破坏内核和另一个进程；调度、退出和非法特权指令都有串口证据。

### G3：IPC、共享内存与 capability

实现同步 endpoint、小消息 copy-in/copy-out、超时/取消、共享内存映射和 capability table。capability 是内核对象引用、generation、rights、owner 和撤销 lineage 的组合；句柄猜测、越权转移、过期 generation 和撤销后的旧句柄都必须失败。

验收：两个用户进程完成结构化 PING/PONG；共享页只在授权范围可见；capability 可降权、转移和撤销；坏长度、跨页指针、队列满和冻结取消均有负向测试。

### G4：Supervisor 与服务启动骨架

从只读 initrd 启动第一个用户态 Supervisor。定义服务 manifest、依赖 DAG、heartbeat、READY/FAULT/EXIT/FROZEN 生命周期、任务进程组、capability bootstrap、重启退避和日志。Supervisor 不能自行制造 policy root 或 kernel admin capability。

验收：故意崩溃的服务被回收并重启；旧 capability 和 IPC 句柄失效；任务树可以冻结、恢复和终止；内核继续运行。

当前增量：clean `SYS_EXIT` 会撤销旧 capability，并在父级授权 `SYS_RESTART` 时 bounded re-mint；原生 READY gate 已验证服务先阻塞等待依赖，再由 Supervisor 释放。证据见 `learning/evidence/g4-clean-exit-capability-remint.md` 与 `learning/evidence/g4-supervisor-ready-gate.md`。

### G5：UEFI 产品启动器与 BootInfo v2

编写最小 PE/COFF UEFI loader，使用 FAT ESP 读取同一 ELF64，获得 UEFI memory map、GOP、ACPI RSDP，有限次处理 `ExitBootServices` key 变化，填充 BootInfo v2 后跳入同一个内核入口。退出 Boot Services 后内核不保留 EFI boot-service 指针。

验收：OVMF 与 SeaBIOS 都进入同一内核 marker；坏 ELF、缺失文件、map key 失效和可选 GOP/ACPI 缺失均有明确降级或 fail-closed 行为；UEFI 路径不依赖 BIOS 中断。

### G6：virtio 与基础系统服务

先支持 QEMU virtio-block、virtio-net、virtio-input/serial 的最小传输和复位，再把文件/存储、网络、输入/窗口、进程服务放在用户态，通过 IPC、shared memory 和 capability 协作。提供 initramfs fallback，内核只保留硬件边界和必要中断机制。

验收：从 virtio-block 读取服务或文件；网络 loopback fixture 可用；输入事件进入窗口服务；单服务崩溃不会拖垮其他服务；不宣称真实硬件驱动覆盖。

当前增量：modern virtio capability layout、BAR 范围安全、Ring 3 RX buffer handoff ABI 和负向契约已固定并纳入测试；这些仍是 host/协议边界，virtio-net RX used completion、modern virtio-input event queue 和 DMA/IOMMU 校验尚未完成。

最新增量：`g6-virtio-net-rx-test.sh` 已在 QEMU socket backend 通过真实 `used.idx` RX completion；输入路径已完成独立 queue arm/poll，QMP synthetic event 仍明确 BLOCKED，modern event completion 与 DMA/IOMMU 仍未验证。

### G7：Policy Firewall、一次性 token 与事务恢复

定义包含 actor、task、capability、resource、risk、reversibility、expiry、audit id 的动作协议。Policy Firewall 负责 L0–L3 判断和 Trusted Input；内核验证并一次性消费绑定范围的 token。文件等本地动作写入 journal/snapshot，外部副作用标记为不可自动重放；全局暂停执行 revoke → freeze → cancel IPC → stop input → persist log。

验收：伪造、重放、过期、错资源 token 全部拒绝；L0/L1/L2/L3 行为符合策略；崩溃后日志可恢复且无重复外部副作用；紧急暂停能冻结整个任务树。

### G8：Semantic Registry 与 Context Collector

实现用户态 registry，保存稳定 tool id、版本、参数 schema、capability 要求、风险、可逆性、结果验证器和 provider。查询按意图、标签、能力和版本确定性返回结构化动作；Context Collector 只提交当前任务窗口和明确授权文件，执行前后都保留证据。

验收：schema 不匹配、未知工具、未授权注册/撤销和越界上下文在执行前拒绝；版本 pin、废弃和 provider 崩溃有明确行为。

### G9：Agent Runtime 适配层

Ring 3 runtime 优先以 pi agent 的轻量 agent loop、tool calling 和状态管理为适配基线；按需吸收 OpenAI Codex app-server 的双向 JSON-RPC、线程/turn/item 生命周期、背压、审批和事件流语义。具体依赖版本、许可证和可移植性在实现时重新核验。

Runtime 只实现：session/task-window、上下文收集、远程 model adapter、结构化 ActionPlan、registry lookup、policy/token、native syscall/IPC executor、postcondition verifier、journal/recovery、Supervisor heartbeat。模型输出永远是不可信数据，不能直接执行 shell 或生成 capability。

验收：mock remote model 能完成 L0/L1 fixture；提示注入和 malformed plan 被拒绝；L2/L3 等待 Trusted Input；断网进入 offline/null adapter；runtime 崩溃后 Supervisor 重启；每个动作都有结果证据和日志。

当前增量：可选 `@earendil-works/pi-agent-core@0.85.1` Ring 3 host adapter 已接入 `Agent.subscribe()`/`prompt()`，并通过真实 `Agent/event stream` smoke 将响应校验为 versioned `ActionPlan`；这不等同于远程 provider、原生 Agent Runtime 服务或 QEMU 端到端完成。

最新增量：原生 Ring 3 Runtime service 已在 BIOS/QEMU 与 OVMF 通过真实 IPC envelope 完成 `START → HEARTBEAT → ACK → CHECKPOINT`，ABI 偏移断言与 scheduler idle 证据已加入；这仍不是完整 Registry、远程模型、持久 checkpoint 或 G10 故障矩阵。

### G10：端到端演示与发布审计

在 QEMU 上完成一条可录制任务：用户输入目标 → Context Collector → mock/remote model → Registry → Policy → capability/syscall/IPC → 文件或窗口服务 → 后置验证 → journal。再执行服务崩溃、agent 超时、token 重放、紧急暂停、回滚和离线模式故障注入。

验收：BIOS/UEFI 启动、两个进程、Supervisor、策略、服务和 Agent Runtime 证据能在同一套 `run-all` 中重现；文档逐项标注已验证、部分验证和未验证。

## 依赖顺序与并行边界

```text
G0 → G1 → G2 → G3 → G4 ─┬→ G6 → G7 → G8 → G9 → G10
                         └→ G5 ────────────────────────┘
```

G5 可以在 G3/G4 之后并行，但必须在 G10 前完成。G6 不能早于 G3 的 IPC/capability，G7 不能早于服务资源接口，G9 不能早于 G4/G7/G8。

## 完成定义

“整体框架完成”只表示 G0–G10 的 QEMU/OVMF 可验证纵向闭环完成，不表示生产级硬件支持。以下内容明确留在后续路线：完整 GPU/USB/NVMe/Wi‑Fi、电源管理、多用户、SMP、完整桌面生态、POSIX/Windows/Linux 兼容和未配置凭据的真实远程模型服务。

## 参考实现边界

- [pi](https://github.com/earendil-works/pi)：参考 `@earendil-works/pi-agent-core` 的 agent loop、tool calling、状态与事件流；MIT 许可证，接入前固定版本并保留 NOTICE。该项目本身不提供本 OS 的权限边界，能力检查仍由 Policy 和内核完成。
- [OpenAI Codex app-server](https://github.com/openai/codex/tree/main/codex-rs/app-server)：参考 JSON-RPC transport、thread/turn/item 生命周期、审批、事件流和背压；只复刻协议语义和适配边界，不复制不兼容实现。
- Linux、xv6、JOS、seL4、MINIX、Limine/GRUB、TianoCore：只读对照硬件初始化、ABI、错误处理和启动流程，记录来源与许可证。

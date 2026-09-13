# Agent-First OS 实施计划

## 目标

在 `x86_64 + QEMU` 上从零建立一个可独立启动的 Agent-First OS 基础。第一阶段只负责证明启动链、内核入口、用户态边界和最小通信原语能够被自己实现、观察和验证；agent、远程模型、桌面和真实 PC 驱动在后续阶段接入。

## 当前基线

- 工作树：当前 Codex 聊天分支；Git 分支保持 `main`
- 产品架构：混合内核，Ring 0 + Ring 3
- 内核语言：C + x86_64 汇编
- 教学启动：SeaBIOS，自写 Stage 1/Stage 2
- 产品启动：UEFI/OVMF，复用同一 `ELF64 + BootInfo` 内核契约
- 开发硬件：QEMU，先单核；串口作为自动化证据通道
- 工具链：WSL2 中的 `gcc`、`nasm`、GNU `ld`、`objcopy`、`readelf`、`qemu-system-x86_64`
- 编译策略：freestanding、无 libc、无 PIE、无 red-zone；后续视需要建立 `x86_64-elf` 交叉工具链

## 里程碑

### M0：工具链和构建契约

产物：可复现的 WSL 构建入口、构建目录约定、QEMU/SeaBIOS 参数、串口日志路径、`readelf/objdump` 证据命令。

验收：同一工作树可以从 PowerShell 调度 WSL 构建；构建失败能区分工具链、汇编、链接和镜像阶段；生成文件不污染源码目录。

### M1：BIOS Stage 1

产物：一个 512 字节启动扇区，设置段寄存器和栈，清除 DF，保存 BIOS `DL`，显示 `S1` 标记，包含 `0x55AA` 签名。

验收：SeaBIOS 从 raw 镜像启动后能看到串口或屏幕标记；错误签名和无效镜像能被诊断；不宣称已经进入保护模式。

### M2：Stage 2 和 EDD 读取

产物：Stage 1 加载位于固定 LBA 的 Stage 2；Stage 2 使用 EDD `INT 13h/AH=42h` 读取后续扇区，带 DAP 对齐、读取上限、重试、复位和错误输出。

验收：正常镜像出现 `S1 → S2`；改坏 LBA、截断 Stage 2 或制造读取失败时不会静默挂死；Stage 2、栈和 BootInfo 暂存区互不覆盖。

### M3：32 位到 64 位

产物：A20、GDT、保护模式、CPUID/PAE 检查、identity 页表、长模式 GDT、64 位入口和对齐栈。

验收：GDB 可以在模式切换和 64 位入口断点；串口记录失败阶段；非法页表或不支持 long mode 时进入明确 halt 路径。

### M4：ELF64 和 BootInfo

产物：只处理合法 ELF64 little-endian x86_64 `PT_LOAD` 的加载器；清零 BSS；传入版本化、定宽字段的 `BootInfo`。

验收：正常 ELF 完成段装载并记录入口跳转；错误 magic/class、`p_filesz > p_memsz`、整数溢出、入口不可执行和地址越界被拒绝。段重叠拒绝和更完整的 ELF 负向矩阵列入后续强化测试。

### M5：C 内核骨架和异常观测

产物：linker script、`.text/.rodata/.data/.bss` 布局、C `kernel_main`、串口日志、IDT 最小异常入口、故障转储。

验收：可以解释并复现启动地址、栈位置和 BootInfo 生命周期；除明确初始化的中断入口外不宣称已有完整驱动或调度。

### M6：单核物理页与虚拟内存

产物：E820 解析、物理帧分配、页映射和恒等映射到高半内核的迁移准备。

验收：故意破坏页表时能得到可解释的 page fault 或 halt；高半映射启用后内核代码、栈、BootInfo 和页表地址语义清楚。

### M7：Ring 3、静态 ELF 和 syscall

产物：用户地址空间、静态 ELF 用户程序、Ring 3 入口、最小 syscall ABI、用户指针检查和退出回收。

验收：用户程序能输出文字并退出；非法用户指针和特权指令不会破坏内核；GDB/串口能证明用户态到内核态再返回的链路。

### M8：IPC、共享内存和 capability

产物：同步消息端点、共享内存映射、不可伪造句柄、权限位、转移和撤销。

验收：两个 Ring 3 进程能交换结构化消息；撤销 capability 后旧句柄失效；共享内存范围和生命周期可验证。

### M9：Supervisor 和用户态服务骨架

产物：第一个用户态进程、服务 manifest、启动依赖、任务进程组、崩溃重启和日志。

验收：一个用户态服务故意崩溃时内核和其他服务继续工作；Supervisor 能撤销任务能力并冻结任务树。

### M10：UEFI 产品启动器

产物：UEFI/OVMF 启动器，读取 FAT ESP 中的 ELF64 内核，获取内存图、GOP、ACPI，正确处理 `ExitBootServices`，填写同一版 `BootInfo`。

验收：BIOS 和 UEFI 两条路径进入同一个 `kernel_entry`；内核不依赖 BIOS 中断或 UEFI boot-service 指针。

### M11–M14：系统服务和 Agent Runtime

后续依次加入 virtio、文件/进程/输入/网络服务、L0–L3 策略和一次性 token、事务日志与恢复、Semantic Registry、远程模型适配器。每个阶段必须先有原生接口和故障注入，再连接 agent。

## 第一轮执行范围

本轮先执行 `M0 → M7`：

1. 建立目录和构建契约；
2. 生成最小 BIOS raw 镜像；
3. 自写 Stage 1 和 Stage 2；
4. 验证从 BIOS 进入 64 位入口的关键节点；
5. 将同一镜像中的 ELF64 内核装载到物理地址，填充 `BootInfo` 并进入 C 入口。
6. 安装最小 IDT，记录可复现异常并在故障路径停机。
7. 建立最小 Ring 3 入口、TSS/RSP0 和 syscall 负向验证。

## 当前状态

整体框架的长链路执行规格已迁移到 [`OVERALL_FRAMEWORK_PLAN.md`](OVERALL_FRAMEWORK_PLAN.md)。该文档的 G0–G10 是当前 goal 的验收顺序；本文件保留 M0–M14 的教学映射。

- **M0：通过**。WSL 构建入口、PowerShell 调度和输出目录已建立；工具版本和命令证据见 `learning/evidence/m1-stage1.md`。
- **M1：通过**。有效启动扇区能在 SeaBIOS/QEMU 中输出 marker；破坏 `0x55AA` 后不会进入 Stage 1。证据见 `learning/evidence/m1-stage1-qemu.txt`、`learning/evidence/m1-stage1-bad-signature.txt` 和 `learning/evidence/m1-stage1-disassembly.txt`。
- **M2：通过**。Stage 1 使用 EDD 读取 Stage 2，正常镜像完成 `S1 → S2`；错误 LBA 会重试、诊断并停止。证据见 `learning/evidence/m2-stage2.md` 和 `learning/evidence/m2-stage2-qemu.txt`。
- **M3：通过**。Stage 2 检查 CPUID/long mode，启用 A20、GDT、PAE、EFER、identity 页表并进入 64 位代码；`qemu32` 负向测试能明确报错并 halt。证据见 `learning/evidence/m3-long-mode.md`。
- **M4：通过（基础版）**。Stage 2 从固定 LBA 读取 ELF64，校验 x86_64 little-endian 头、程序头边界、段文件/内存大小和 1–2 MiB 物理范围，清零 BSS，填充版本化 `BootInfo`，再跳转到 C 内核；损坏 ELF 会被拒绝。证据见 `learning/evidence/m4-elf-bootinfo.md`。
- **M5：通过（基础版）**。C 内核安装 256 项 IDT，异常桩统一错误码布局并记录 vector/error/RIP；除零故障测试会进入 IDT 并明确停机。证据见 `learning/evidence/m5-idt.md`。
- **M6：进行中（E820、布局、基础 PMM 与高半别名准备子步通过）**。BIOS E820 条目已收集到低内存缓冲区并传入 `BootInfo`，内核记录当前恒等映射上限和高半目标地址，并有 bounded 多区域保留范围物理页分配器，能跨 E820 usable 区间避让 kernel/staging/initrd/E820 表；BIOS 页表池已经有界使用该 PMM，并为已分配的低地址页表页建立 supervisor-only 别名，PMM 不可用时回退 bootstrap pool；kernel text/rodata/data/bss 已在每个进程根中建立同物理页、同 W^X 权限的高半别名，并有 host 与 BIOS/QEMU 证据；已验证切换到进程 CR3 后执行无状态高半 RIP probe 并回退低地址栈；高半栈、C 数据访问、并发分配、UEFI memory map 和完整内存类型处理仍未完成；单核 PIT 抢占另有 G2 定向证据。证据见 `learning/evidence/m6-e820.md`、`learning/evidence/g1-high-half-alias.md`、`learning/evidence/g2-physical-allocator-vm.md`。
- **M7：通过（基础版）**。静态用户程序通过 DPL3 代码/数据段进入 Ring 3；内核安装 64 位 TSS/RSP0，`int 0x80` 支持 `SYS_WRITE/SYS_EXIT`，并拒绝越界用户指针和未知 syscall。默认启动仍 gated，显式测试宏验证实际用户输出、syscall 和退出回收。证据见 `learning/evidence/m7-ring3.md`。
- **G2 ELF 子步：通过（嵌入式 fixture）**。独立 `user.elf` 经过 Ring 0 ELF64 `PT_LOAD` 校验、复制、W^X 用户映射后进入独立 CR3；通用动态文件装载和多段用户镜像仍未完成。证据见 `learning/evidence/g2-elf-loader.md`。
- **M8–M9：部分内核切片与协议骨架进行中。** G2 已有 BIOS/QEMU distinct-CR3 fixture、嵌入式用户 ELF、IRQ masking 回归和用户 #PF→task recovery；G3 已有真实 `SYS_IPC_CALL`/`SYS_IPC_RECV`、capability-authorized shared-page 映射、restrict/transfer/revoke，以及 capability/IPC/shared-memory 主机负向测试；capability lineage host test 还证明跨表派生链、祖先撤销、branch sibling 隔离、表 incarnation 和 generation 退休，`SYS_IPC_RECV_WAIT` 已证明多个接收者可登记等待序列号，由发送者按 FIFO 跨独立 CR3 唤醒并投递消息，父级 `SYS_IPC_CANCEL` 还证明阻塞子任务能收到 `ECANCELED` 后被 wait/reap。新增四槽 Ring-0 endpoint pool 与 `SYS_IPC_CREATE/DESTROY`，动态 endpoint 已在 BIOS/QEMU 中完成创建、收发、销毁和旧 capability 拒绝，证据见 `product/tests/g3-cap-lineage-test.sh`、`learning/evidence/g3-kernel-multiwait-ipc.md`、`learning/evidence/g3-kernel-ipc-cancel.md`、`learning/evidence/g3-native-ipc-close.md` 和 `learning/evidence/g3-dynamic-endpoint.md`。G4 已有 BIOS 与 UEFI/OVMF version 2 initrd manifest/service table/ELF 装载，再加上 `g4-native-supervisor-test.sh` 的原生 Ring 3 heartbeat → service exit → parent-authorized restart → wait/reap 证据，以及 faulted service→zombie→restart→heartbeat→wait/reap 证据；G7/G9 另有原生 policy token、expiry/emergency pause 和 Agent→Policy IPC 纵切。Supervisor manifest/event、Python Supervisor 参考状态机、Policy/Registry schema、`PiAgentLoop` reference 和 Ring 3 Agent Runtime 参考实现也已有一致性测试。新增原生父级限定、非递归 direct-child task-group terminate 与 wait/reap 证据；freeze/resume 已有原生子集证据；递归 terminate 已有 host 与 test-gated BIOS/QEMU 三层证据；recursive freeze/resume、clean-exit capability re-mint 和完整服务编排仍未完成。
- **M10–M14：部分推进。当前已增加 BIOS/QEMU Ring 3 固定 file-write 纵切、synthetic input/window IPC 边界和受限 G10 原生组合链：Context/Agent 请求、固定 mock-model/Registry marker、Policy 一次性 token、Ring 3 服务消费、后置结果、journal commit、replay 拒绝和故意后置条件失败后的内存回滚；仍不等同于持久化文件系统、真实窗口/网络服务或完整端到端 G10。** G5 已有自写 UEFI loader 的 ELF/BootInfo v2/ExitBootServices/GOP/ACPI/OVMF 运行证据和坏 ELF/map-key 负向测试；G6 已有 virtio-blk sector read、virtio-net 初始化、legacy queue 1 TX 完成与 virtio-input 发现/受门控 queue arm 尝试（当前 QEMU BIOS 路径 BLOCKED），并新增 host-only 文件/网络/窗口/输入服务协议 fixture 与 bounded native input/window IPC；G7/G9/G10 已有 BIOS 与 OVMF 的 native token/Agent loop、policy pause、pi-shaped reference loop、bounded HTTP disconnect retry 和 `g10-native-chain-test.sh`。 Host-side `DurableJSONLJournal` 另有 fsync/reopen/recovery 与 fail-closed 尾部校验证据。 Agent Runtime 另有 host-side durable checkpoint 和 JSON-over-HTTP adapter 定向证据；实际 pi/远程 provider 仍未接入。原生完整服务、持久化策略事务恢复、实际 pi/remote model runtime、完整 G10 故障矩阵仍按 `OVERALL_FRAMEWORK_PLAN.md` 的剩余边界实现。

## 目录约定

```text
product/
  kernel/
    arch/x86_64/
      boot/bios/      # Stage 1 / Stage 2
      boot/uefi/      # 后续 UEFI adapter
      include/        # 内核 ABI 和 BootInfo
      src/            # C 内核实现
  tools/              # PowerShell/WSL 构建、镜像和调试入口
  tests/              # 可运行的功能、负向和故障验证
  docs/               # 设计、计划、决策和发布说明
learning/
  evidence/           # 命令、日志、反汇编、GDB 和故障样本
  projects/           # 教学实验与阶段性对照实现
```

产品代码和学习实验保持分离。实验先在 `learning/projects/` 验证，达到对应 Gate 后再迁移到 `product/`；本轮若直接在产品目录实现，必须同步保存独立运行证据并标记尚未通过的 Gate。

## 构建与验证契约

- PowerShell 只负责调度；实际裸机编译和 QEMU 运行在 WSL2 完成。
- 所有命令显式引用 `/mnt/d/Agent OS`，避免依赖当前 shell 的隐式目录。
- 构建使用 freestanding 参数：`-ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -nostdlib`；禁止依赖 libc、TLS、SSE 和 red-zone，直到内核明确初始化这些设施。
- 自动化 QEMU 使用 `-display none -serial stdio`；图形观察另行使用 GOP/VGA 配置。
- 每个里程碑至少保存：构建命令、工具版本、QEMU 参数、串口日志、`readelf/objdump` 输出和一个负向样本。
- 生成的 `.o/.elf/.bin/.img/.iso/.efi` 和日志进入忽略的构建目录或 evidence 明确允许的路径。
- 未经验证的 BIOS、UEFI、真实 PC、SMP、驱动和远程模型行为不得写成已完成。

## 外部参考边界

参考项目只用于事实、接口和错误处理对照：MIT JOS/x86、xv6、Linux x86/EFI、Limine/GRUB、seL4、MINIX、Redox、SerenityOS、TianoCore/UEFI。默认只读并重写接口；不复制 Linux GPL 实现或第三方二进制。需要迁移代码时，先记录 commit、许可证、改写范围、兼容性影响和来源。

## 当前未决问题

- M0 是否继续使用系统 GCC，还是在完成启动链后建立 `x86_64-elf` 交叉工具链；
- Stage 2 初期使用纯汇编还是在进入 32/64 位后引入 freestanding C loader；
- virtio 驱动暂留内核还是在用户态服务阶段实现；
- UEFI 启动器采用自写 PE/COFF 最小程序还是接入 TianoCore 的构建辅助；
- capability bootstrap 和服务注册协议的最终消息格式。

这些问题不阻塞 M1；一旦影响当前里程碑，会单独记录决策和迁移成本。

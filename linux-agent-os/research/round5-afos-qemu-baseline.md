> 入库说明（方案规格）：本文是 codex工具人 对 agent-first-os 的 QEMU 实测报告原文，仅把工作路径换成了尖括号占位符，内容未改。验收审查核对了报告、日志和原仓库差异，结论为「有条件通过」，没有复跑。它只是测试宏镜像在 QEMU（TCG）下的基线记录，不是 M1 的运行证据。`run-all.sh` 原样跑在第 2 项中止，完整结果来自逐项跑，不得写成全绿。文中引用的 `logs/` 下的日志文件没有入库，只入库了逐项汇总表 `research/round5-run-each-summary.tsv`，并在 `research/round5-key-log-sha256.txt` 里记录了关键日志的 sha256。

# Agent-First OS 在 QEMU 中的只读评估报告

评估时间：2026-11-02 15:37–16:00（Asia/Shanghai）。原仓库路径 <原仓库> 只做了读取；评估用的拷贝在 <评估副本>（拷贝时排除了 .git）。

## 一句话结论
- **BIOS 路径**：能构建，也能启动。串口按顺序打出了 S1 → S2 → long mode → ELF 交接 → IDT → `KERNEL C OK`，run-bios.sh 判定通过。
- **UEFI 路径**：能构建，也能启动。OVMF 加载自写的 BOOTX64.EFI 后，读入 KERNEL.ELF，调用 ExitBootServices，进入共享的 kernel_entry，然后跑 Ring 3 两个任务，最后到 scheduler idle。整条链路都有串口原文作证。
- **run-all.sh 里的 95 项**：逐项单独跑（每项 timeout 300），**86 项通过，9 项失败**。失败集中在两处：virtio-blk/virtio-net 设备探测（8 项），以及 PIT 定时器抢占（1 项）。

## 现状（根据文档）
- product/docs/OVERALL_FRAMEWORK_PLAN.md 的说法：G0–G5 的纵切已经通过；G6–G10 是“受限原生证据 / 进行中”；整体框架还没完成。
- 有两条启动路径：
  - BIOS：`product/tools/build-bios.sh`，产物是 stage1(512B)+stage2(24 扇区)+kernel.elf+initrd 拼成的 raw 镜像，用 `run-bios.sh` 以 `-machine pc` 启动。
  - UEFI：`product/tools/build-uefi.sh`，用 clang 生成 x86_64-pc-windows-msvc 目标，再用 lld-link 链成 BOOTX64.EFI。G5 测试用 OVMF pflash，`-machine q35`，ESP 通过 `fat:rw:` 目录挂载。stage0 只打标记；g5-uefi-kernel 才走完整的 uefi_loader.c → KERNEL.ELF 流程。
- 所有 QEMU 调用都经过 `product/tests/qemu-timeout.sh`：串口写到文件，超时返回 124，这是期望结果。

## 工具链（全部用户态安装，没用 sudo）
见 logs/toolchain-versions.log 和 logs/toolchain-install.log。
- nasm 2.16.03、clang 23.1.4、LLD 23.1.4（自带 lld-link）、file 5.48：用 micromamba（复用 <工具目录>/bin/micromamba）从 conda-forge 装进 <评估目录>/env。
- QEMU 10.0.13（Debian 13 版）：复用 landlock-proto/tools/qemu-root，没改动它。包装脚本 bin/qemu-system-x86_64 负责设置 LD_LIBRARY_PATH，并用 `-L` 指定 qemu 和 seabios 的数据目录。
- OVMF：Debian trixie 的 `ovmf_2025.02-8+deb13u1_all.deb`，sha256 是 78e0d54df11fc77406cb7a0bc9a39e5bca6d1cbe06556b91d9a73491c52decdf，用 dpkg-deb -x 解到 ovmf/root。
- gcc 14.2.0；ld 用的是 /usr/bin/ld（env.sh 把 /usr/bin 放在 conda env 前面）；python 3.13.5；node v24.22.1（盒子自带）。
- /dev/kvm 属于 root:pulse-access，当前用户没有权限，所以全程用 TCG（QEMU 的默认加速方式）。

## 对拷贝做的改动（只动了 src 拷贝，没动原仓库）
1. **chmod +x**：99 个 .sh 在 git 里登记的模式是 100644（清单见 logs/chmod-x-files.txt）。`run-bios.sh` 直接执行 `qemu-timeout.sh`，第一次运行报错原文：`product/tools/run-bios.sh: line 15: <评估副本>/product/tests/qemu-timeout.sh: Permission denied`（logs/bios-run-attempt1-noexec.log）。这是**仓库本身的问题**：在 WSL 的 /mnt/d 上所有文件看起来都可执行，所以作者那边不会暴露。
2. **OVMF 路径**：9 个 UEFI 测试把 `/usr/share/OVMF` 写死了，我用 sed 替换成 ovmf/root/usr/share/OVMF（文件清单见 logs/ovmf-patched-files.txt）。属于盒子环境缺东西，不是代码问题。
3. **`mkdir -p build/uefi`**：g5-uefi-stage0-test.sh 在第 6 行把输出重定向到 `$out_dir/build-stage0.log`，但这时目录还没建（目录由 build-uefi.sh 创建，可重定向发生在它运行之前）。在全新目录里第一次运行的报错原文：`product/tests/g5-uefi-stage0-test.sh: line 6: <评估副本>/build/uefi/build-stage0.log: No such file or directory`（logs/test-g5-uefi-stage0-attempt1-nomkdir.log）。这是**脚本缺陷**。在 run-all 的顺序里，前面的测试会先把这个目录建好，所以平时被掩盖了。

## 关键实测
| 步骤 | 命令 | 结果 | 日志 |
|---|---|---|---|
| BIOS 构建 | bash product/tools/build-bios.sh | 通过（0.34s） | logs/bios-build.log |
| BIOS 启动 | bash product/tools/run-bios.sh | 通过 | logs/bios-run.log, logs/bios-serial.log |
| UEFI stage0 | product/tests/g5-uefi-stage0-test.sh | 先建好 build/uefi 后通过 | logs/test-g5-uefi-stage0-test.log, logs/uefi-stage0-serial.log |
| UEFI→内核→Ring3 | product/tests/g5-uefi-kernel-test.sh | 通过（12s） | logs/test-g5-uefi-kernel-test.log, logs/uefi-kernel-serial.log |
| run-all.sh 原样跑 | timeout 1800 bash product/tests/run-all.sh | 失败。它在第 2 项 g6-virtio-net-rx 停下（脚本遇错即退） | logs/run-all.log |
| 逐项跑 | ./run-each.sh | 86/95 通过 | logs/run-each-summary.tsv, logs/tests/*.log |

BIOS 串口原文（摘录）：
```
S1 BIOS OK - stage2 pending
S2 BIOS OK - stage2 loaded
S2 64BIT OK - long mode
KERNEL ELF OK - jumping to kernel
VM G1 4K NX map OK
IDT OK - 256 vectors
KERNEL C OK
boot_info.loader_type=0x0000000000000001
RING3 ABI READY - launch gated pending TSS
```
UEFI 串口原文（摘录，logs/uefi-kernel-serial.log）：
```
AGENTOS UEFI LOADER
AGENTOS UEFI ELF OK
AGENTOS UEFI INITRD OPTIONAL ABSENT
AGENTOS UEFI EXIT BOOT SERVICES
VM G1 4K NX map OK
IDT OK - 256 vectors
KERNEL C OK
boot_info.loader_type=0x0000000000000002
G2 ELF user load OK
USER RING3 OK
USER RING3 TASK2 OK
SCHEDULER idle - all tasks exited
```

## 失败项（9 项，都是测试断言失败，构建和启动本身都成功）
| 测试 | 现象 | 判断 |
|---|---|---|
| g6-virtio-net-rx, g6-virtio-net-tx | 串口里完全没有 `G6 virtio net transport READY` | 见下面的 A |
| g6-virtio-block | 有 `G6 virtio input transport READY`，但没有 block/net READY | A |
| g7-native-block-write/flush/read | 串口出现 `SCHEDULER block device capability unavailable` | A 的连带结果 |
| g7-native-journal-recovery | `journal prepare marker was not observed`，前面一行是 `SCHEDULER block device capability unavailable` | A 的连带结果 |
| g7-native-journal-commit-recovery | `journal commit marker was not observed`，同样缺 block 设备 | A 的连带结果 |
| g2-irq | 已到 `SCHEDULER idle - all tasks exited`，但缺 `TIMER PREEMPT switched task`；用 `-d int` 的 trace（build/bios/qemu-g2-irq-trace.log）里 `v=20` / `v=08` 都是 0 次 | 见 B |

**A（virtio-blk/net 探测失败）**：我用同一镜像和同样的设备参数，在 QEMU monitor 里执行 `info pci`，看到 `1af4:1001`（blk）的 `BAR0: I/O at 0xc000`，`1af4:1000`（net）的 `BAR0: I/O at 0xc080`。也就是说设备在，legacy I/O BAR 也已分配。所以失败点在 `product/kernel/src/virtio_pci.c` 的 `probe_products()`：它会在 FEATURES_OK、queue_size、DRIVER_OK 几个检查中的某一处 `continue` 掉。**具体卡在哪一处，我没继续定位。** 可能是 QEMU 10.0.13 和作者环境里的 QEMU 版本行为不同（作者用的版本未知），但**这一点没验证**，不能下结论。
**B（g2-irq）**：在本次 TCG 运行的 5 秒窗口里，没有观察到 IRQ0 送达。原因没查明（可能是代码问题，也可能是 TCG 时序或 QEMU 版本差异），**未验证**。

## 没跑或没验证的
- 没用 KVM（没有权限）。
- run-all.sh 里的 node 测试都跑了，也都通过（node 是盒子自带的）。
- 上面 A、B 两处的根因都没深挖；没有换别的 QEMU 版本做对照。
- 9 个 UEFI 测试用的是替换过的 OVMF 路径，没用系统里的 /usr/share/OVMF（盒子上不存在这个目录）。

## 复跑方法
```
. <评估目录>/env.sh
cd <评估副本>
bash product/tools/build-bios.sh && bash product/tools/run-bios.sh
mkdir -p build/uefi && bash product/tests/g5-uefi-stage0-test.sh
bash product/tests/g5-uefi-kernel-test.sh
<评估目录>/run-each.sh > <评估目录>/logs/run-each-summary.tsv
```

## 逐项结果（run-each-summary.tsv）
| 测试 | rc | 耗时 |
|---|---|---|
| g6-mmio-map-test.sh|rc=0|0s |
| g6-virtio-net-rx-test.sh|rc=1|6s |
| g6-virtio-input-test.sh|rc=0|15s |
| g0-contract-test.sh|rc=0|0s |
| g1-high-half-test.sh|rc=0|4s |
| g2-physical-allocator-test.sh|rc=0|0s |
| g2-process-test.sh|rc=0|0s |
| g2-address-space-test.sh|rc=0|0s |
| g2-elf-loader-test.sh|rc=0|0s |
| g2-irq-test.sh|rc=1|6s |
| g2-kernel-test.sh|rc=0|3s |
| g2-wait-kill-test.sh|rc=0|3s |
| g2-fault-recovery-test.sh|rc=0|4s |
| g3-ipc-caps-test.sh|rc=0|0s |
| g3-cap-lineage-test.sh|rc=0|0s |
| g3-shm-test.sh|rc=0|0s |
| g3-dynamic-endpoint-test.sh|rc=0|0s |
| g3-endpoint-destroy-lineage-test.sh|rc=0|0s |
| g3-kernel-ipc-test.sh|rc=0|3s |
| g3-kernel-blocking-ipc-test.sh|rc=0|5s |
| g3-kernel-multiwait-ipc-test.sh|rc=0|4s |
| g3-kernel-ipc-cancel-test.sh|rc=0|5s |
| g3-native-ipc-close-test.sh|rc=0|4s |
| g3-native-dynamic-endpoint-test.sh|rc=0|4s |
| g3-kernel-shm-test.sh|rc=0|4s |
| g3-kernel-capability-test.sh|rc=0|3s |
| g3-kernel-cap-transfer-test.sh|rc=0|3s |
| g4-cap-reclaim-test.sh|rc=0|0s |
| g4-clean-restart-cap-test.sh|rc=0|1s |
| g4-group-recursive-test.sh|rc=0|0s |
| g4-native-recursive-task-group-test.sh|rc=0|4s |
| g4-native-supervisor-test.sh|rc=0|4s |
| g4-supervisor-ready-gate-test.sh|rc=0|5s |
| g4-supervisor-fault-restart-test.sh|rc=0|4s |
| g4-task-group-test.sh|rc=0|0s |
| g4-native-task-group-test.sh|rc=0|5s |
| g4-group-freeze-test.sh|rc=0|0s |
| g4-native-group-freeze-test.sh|rc=0|4s |
| g4-uefi-initrd-test.sh|rc=0|13s |
| g4-multi-initrd-test.sh|rc=0|6s |
| g5-uefi-stage0-test.sh|rc=0|8s |
| g5-uefi-kernel-test.sh|rc=0|12s |
| g5-uefi-map-key-test.sh|rc=0|13s |
| g5-uefi-negative-test.sh|rc=0|8s |
| g6-virtio-block-test.sh|rc=1|6s |
| g7-native-block-write-test.sh|rc=1|5s |
| g7-native-block-flush-test.sh|rc=1|5s |
| g7-native-block-read-test.sh|rc=1|6s |
| g7-native-journal-recovery-test.sh|rc=1|6s |
| g7-native-journal-commit-recovery-test.sh|rc=1|7s |
| g6-virtio-input-bar-contract-test.sh|rc=0|0s |
| g6-virtio-modern-caps-test.sh|rc=0|0s |
| g6-virtio-rx-protocol-test.sh|rc=0|0s |
| g6-virtio-rx-abi-test.sh|rc=0|0s |
| g6-virtio-net-tx-test.sh|rc=1|6s |
| g6-native-file-ipc-test.sh|rc=0|4s |
| g6-native-file-write-test.sh|rc=0|5s |
| g6-native-io-test.sh|rc=0|5s |
| g6-native-input-window-test.sh|rc=0|4s |
| g7-kernel-token-test.sh|rc=0|5s |
| g7-kernel-pause-test.sh|rc=0|4s |
| g9-native-agent-test.sh|rc=0|4s |
| g9-runtime-service-abi-test.sh|rc=0|0s |
| g9-native-runtime-service-test.sh|rc=0|16s |
| g9-uefi-runtime-service-test.sh|rc=0|12s |
| g10-uefi-agent-test.sh|rc=0|13s |
| g5-g6-contract-test.sh|rc=0|0s |
| stage1-test.sh|rc=0|6s |
| stage2-test.sh|rc=0|3s |
| stage3-test.sh|rc=0|4s |
| stage4-test.sh|rc=0|3s |
| stage5-test.sh|rc=0|3s |
| stage7-test.sh|rc=0|7s |
| stage8-test.sh|rc=0|7s |
| g1-permission-test.sh|rc=0|10s |
| supervisor-protocol-test.sh|rc=0|1s |
| supervisor-runtime-test.sh|rc=0|0s |
| supervisor-g4-group-test.sh|rc=0|0s |
| g6-service-test.sh|rc=0|0s |
| g8-native-semantic-test.sh|rc=0|4s |
| g10-uefi-semantic-test.sh|rc=0|12s |
| g10-native-chain-test.sh|rc=0|6s |
| g10-uefi-chain-test.sh|rc=0|12s |
| g7-g10-host-loop-test.sh|rc=0|0s |
| g7-durable-journal-test.sh|rc=0|0s |
| g9-durable-checkpoint-test.sh|rc=0|0s |
| g9-host-boundary-test.sh|rc=0|0s |
| g8-native-semantic-negative-test.sh|rc=0|4s |
| g7-native-journal-contract-test.sh|rc=0|0s |
| g9-http-adapter-test.sh|rc=0|0s |
| g10-disconnect-recovery-test.sh|rc=0|0s |
| conformance.test.js|rc=0|0s |
| g9-adapter-test.js|rc=0|0s |
| g9-pi-loop.test.js|rc=0|0s |
| g9-pi-core-adapter-test.sh|rc=0|0s |

## 原仓库没被改动的证明
```
$ git -C <原仓库> status --short
(输出为空，exit 0)
```

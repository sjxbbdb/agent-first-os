> 入库说明：验收审查 t-m1-0-r1 的数据记录原文，只做路径、构建用户名、账号的替换（见 `research/round7-stage-reviews.md` 第二节）。日志文件没有入库，哈希见 `research/round7-review-logs-sha256.txt`。

# T-M1-0 复跑核查数据（默认单一 UEFI 镜像与基线冻结）

- 核查时间：2026-11-02 16:16–16:30（UTC+8，盒子本地时间）
- 被审对象：work 仓库 f66c5d7（主体 b4496fc，基于 0066bee）；冻结目录 afos-m1-t0-0.1.0-dev-f66c5d7
- 规格来源：<规格仓库>，`git fetch` 后 origin/main = d629699，只用 `git show origin/main:...` 读取，未动工作区
- 所有日志：<T-M1-0复核目录>/logs/；临时检出：<临时目录>/{a,b,c,n,p,s}
- 环境：`. <评估目录>/env.sh`；AFOS_OVMF_CODE/VARS=<评估目录>/ovmf/root/usr/share/OVMF/OVMF_{CODE,VARS}_4M.fd；AFOS_OVMF_VERSION="Debian ovmf 2025.02-8+deb13u1"
- 未修改被审源码与 <评估目录>/m1-t0 下任何文件；回归脚本复制到 <临时目录>/regress.sh，只改了 cd 目录和日志路径（diff 见 logs/regress-script.diff）

## 1. 干净目录复现构建
命令（logs/item1-summary.txt、logs/build-*.log）：
```
date; git clone <评估目录>/m1-t0/work <临时目录>/{a,b}; git checkout f66c5d7
git rev-parse HEAD; git status --porcelain | wc -l
bash product/tools/build-m1-default.sh   # a、b 各一次，a 再重建一次
env -i HOME=$HOME PATH=/usr/bin:/bin bash product/tools/build-m1-default.sh   # <临时目录>/c，不加载 env.sh
```
关键输出：
- 16:16:04 开始；a、b 的 HEAD 都是 f66c5d74783c6e7c0f54cccec8dc278124ad0a79，porcelain 0 行
- 三次构建 exit=0。耗时：盒子上没有 bc，计时变量没算出来；按 date 时间戳，3 次构建合计约 1.7 s（16:16:15→16:16:16）
- sha256（a、b、冻结 image/ 三方一致，a 重建也一致）：
  - afos-m1-uefi.img 15ec6371…0b2514
  - BOOTX64.EFI 1b78c503748656f966bf01463681a640db298e249aeeb25e8778e4ed2e2b0514
  - kernel.elf 3e2523b8…288b2b，initrd.bin 0da1036f…591c46，build-info.txt、afos_version.h 也一致（logs/item1-hashes.txt 有全值）
- 时间戳检查：build-info.txt 只有 `AFOS-M1 version=0.1.0-dev build=f66c5d7` 和 `image=afos-m1-uefi.img`；kernel.elf 的 .comment 只有 `GCC: (Debian 14.2.0-19) 14.2.0`；对 kernel.elf/BOOTX64.EFI/img 用 strings 搜日期、时刻、<临时目录>、<工作区>，0 命中。FAT 目录项时间字段都是固定值（0x2100 = 1980-01-01）。lld-link 带 /Brepro。
- 不加载 env.sh 构建：exit=127，`build-m1-default.sh: line 21: nasm: command not found`。盒子 /usr/bin 里没有 nasm/clang/lld-link，靠 env.sh 提供；没有缺 OVMF 变量的问题（构建不用 OVMF）。
- 注意：构建脚本本身不打印命令行和退出码；冻结日志里的 `cmd:` 首行和 `exit=` 末行是脚本外的包装加上的，仓库里没有这个包装。

结论：**符合**（两路径 + 重建字节一致，与冻结一致，产物无时间戳）。缺陷线索：工具链靠盒子专属 env.sh，脚本缺工具时没有提示；构建日志的 cmd/exit 不是仓库脚本产生的。

## 2. 启动复现
命令：a、b 各 `bash product/tools/run-m1-uefi.sh` 两次，共 4 次（a1、b1 并发，a2、b2 串行），日志复制在 logs/runs/。
关键输出（logs/item2.txt）：
- 4 次 exit=0、qemu_exit=124；4 份 serial.log sha256 都是 66e01334c4ed71aad5cce289e857a7910af3865c8de9298311c3d5e20498d631，1046 字节，与冻结 clean-c/run-001、run-002、clean-d/run-001 一致，`cmp` 全部通过
- 串口第 9/10 行 `AGENTOS UEFI EXIT BOOT SERVICES`（打了两次），第 13 行 `KERNEL C OK`，第 14 行 `AFOS-M1 version=0.1.0-dev build=f66c5d7`，末行 `RING3 ABI READY - launch gated pending TSS`
- 日志头字段（对照门槛 8）：accel=tcg ✓；qemu_version=QEMU emulator version 10.0.13 (Debian 1:10.0.13+ds-0+deb13u1) ✓；ovmf_code 路径 + sha256 ✓；ovmf_vars 路径 + sha256 ✓；ovmf_version ✓（但只是 AFOS_OVMF_VERSION 环境变量的自由文本，不设就写 `unknown`）；image 路径 + sha256 ✓；kernel_version=0.1.0-dev build=f66c5d7 ✓（取自镜像旁边的 build-info.txt，不是从镜像或串口读的）；qemu_cmd 完整命令行 ✓；qemu_exit ✓
- 串口用 `-serial file:<base>.serial.log` 全量写；QEMU 的 stdout/stderr 另写 qemu-stderr.log（只有一行 SIGTERM 记录）；没有 tee，也不是截取 stdout ✓

结论：**符合**。次要：ovmf_version 和 kernel_version 都靠外部输入，不是实测；qemu_version 是包装脚本的输出（报告已说明）。

## 3. 冻结核对
命令（logs/item3.txt、logs/freeze-check.txt、logs/item3-patches.txt）：
```
cd freeze/afos-m1-t0-0.1.0-dev-f66c5d7 && sha256sum -c SHA256SUMS > <临时目录>/freeze-check.txt
find . -type f ! -name SHA256SUMS 与 SHA256SUMS 条目做 comm；stat；touch ./x
git checkout 0066bee; git apply --check 0001; git apply 0001; git apply --check 0002; git apply 0002; git add -A; git write-tree
```
关键输出：
- sha256sum -c exit=0，不是 OK 的 0 行；条目数 134 ✓；SHA256SUMS 自身 sha256 dfa2efb8…939154，与报告一致
- 不在清单里的文件：无；清单里有但不存在的：无
- 权限：目录 dr-xr-xr-x、文件 -r--r--r--，`touch` 报 Permission denied。但属主就是 构建用户（uid 1000），父目录 freeze/ 是 drwxr-xr-x，属主自己能 chmod 回去或改名。也就是只靠 chmod a-w，没有 chattr +i，也没有别的保护
- BASELINE.txt：baseline=AFOS-M1 version=0.1.0-dev build=f66c5d7；commit=f66c5d7…；base_commit=0066bee (code identical to 85011ca/d629699 outside linux-agent-os)；frozen_at=2026-11-02 16:12:48 CST；nasm=…
- 冻结 image/ 与我的构建：6 个文件全一致（见第 1 节）；冻结 scripts/ 里 6 个脚本与 f66c5d7 的 product/tools 一致，qemu-timeout.sh 与 product/tests/qemu-timeout.sh 一致
- 补丁：0001、0002 依次 `--check` OK、apply OK；应用后 write-tree = 110af665a1577ceaf013b0487d5e1a06b49a1b40，等于 f66c5d7^{tree}，所以 linux-agent-os 目录也一致（含 env.sh 的模式变更）
- 补做：在新检出 <临时目录>/s（没有 build/）里直接 `./product/tests/g5-uefi-stage0-test.sh`，PASS，exit=0（logs/item3-g5-stage0-fresh.log）

结论：**符合**。次要：「只读」只是文件模式，属主自己就能改回来。

## 4. 改动面
命令：`git diff --stat/--numstat/--summary 0066bee f66c5d7`，`git log --format`，`git ls-tree -r`，以及与 origin/main(d629699) 对比（logs/item4*.txt）。
- 105 个文件，+312/-20。有内容改动的 16 个：
  - kernel.c +2：`#include "afos_version.h"`，`KERNEL C OK` 后加一行版本输出（1406 行）
  - 9 个 UEFI 测试（g10-uefi-agent/chain/semantic、g4-uefi-initrd、g5-uefi-kernel/map-key/negative/stage0、g9-uefi-runtime-service）：OVMF 路径改成 `${AFOS_OVMF_CODE:-…}`/`${AFOS_OVMF_VARS:-…}`；g5-uefi-stage0 另加 `mkdir -p "$out_dir"`
  - build-bios.sh +22/-1：nasm 包装（cd 到仓库根，参数改成相对路径）、生成 afos_version.h、kernel.c 编译加 `-I$kernel_build_dir`
  - build-uefi.sh：lld-link 加 /Brepro
  - README.md +14；新增 build-m1-default.sh(56)、run-m1-uefi.sh(65)、make-esp-image.py(133)
  - 其余 89 个只改模式 100644→100755，**其中 1 个在 linux-agent-os/：linux-agent-os/prototypes/proto1-landlock/env.sh**（被 source 的文件，不需要执行位，和 T-M1-0 无关）
- 测试断言/期望值：9 个测试的 diff 只改了 OVMF 路径和 mkdir，没有 grep 期望串或 rc 判定的改动 ✓（门槛 5）
- 作者/提交者：两个提交都是 afos-codex <afos-codex@localhost>
- 与 origin/main：`git diff 0066bee d629699 -- . ':!linux-agent-os'` 为空；`git diff d629699 f66c5d7 -- . ':!linux-agent-os'` 的 stat 与 `0066bee..f66c5d7` 去掉 linux-agent-os 后完全相同（104 个文件）✓
- .sh 模式：f66c5d7 里 111 个全是 100755；0066bee 里 100 个是 100644，9 个 100755（都在 linux-agent-os/prototypes/proto1-landlock/scripts/）

结论：**基本符合，改动面接近最小**。次要：改了 linux-agent-os/ 下 env.sh 的模式，和报告「我只改了产品代码，没有去更新 linux-agent-os/」不完全一致。

## 5. 姓名与路径泄漏
命令：用 `rg -ni` 搜索项目负责人的中文全名、名字两字和名字拼音（大小写不敏感），范围是 diff、提交信息、冻结目录全部文件（-a）、`git grep` f66c5d7 全树和报告；另搜 `/home/` 路径（排除 <构建用户主目录>）和 github.com/<账号>（logs/item5.txt）。
- 中文姓名、拼音（大小写不敏感）：diff、提交信息、冻结目录、全树、报告 **0 命中**
- /home/ 非 构建用户 路径：0 命中
- GitHub 账号（不算姓名，单独列出）：全树中 learning/curriculum/RESOURCES.md、learning/lessons/SF-00-source-baseline.md、learning/source-track/{README,SOURCES,UNIT_TEMPLATE}.md 有 mit-pdos、ChairSleeper、klange、KaiNorberg、AlexanderNyr、raintree-technology、chyyuu；linux-agent-os/research/round1-agent-os-gaps.md:125/141 有 torvalds、仓库所有者的账号（这些都不是本次改动引入的）。冻结目录：build-00{2..6}*.log 里的 github.com/conda-forge（clang/lld 版本串）

结论：**符合**。

## 6. 许可证（门槛 6）
命令：列新增文件的 SPDX、`ls LICENSE*`、`git ls-files | rg -i licen`、rg 头文件和 gnu-efi、读 build-uefi.sh、对镜像做 strings（logs/item6.txt）。
- 新增 3 个文件：build-m1-default.sh、make-esp-image.py、run-m1-uefi.sh，第 2 行都是 `SPDX-License-Identifier: LicenseRef-AgentFirstOS-TBD` ✓
- 根目录没有 LICENSE/COPYING；仓库里有 product/docs/LICENSE_MATRIX.md（自研 product/ 许可证待定，写明没有复制第三方代码；TianoCore 只作参考）。报告没提这份文件
- loader/kernel 的 `<>` 头文件只有 `<stdint.h>`、`<stddef.h>`（编译器自带的 freestanding 头），其余都是仓库自己的 include；没有 gnu-efi/efi.h/efilib
- BOOTX64.EFI：`clang --target=x86_64-pc-windows-msvc -ffreestanding … -c uefi_loader.c`，然后 `lld-link /Brepro /subsystem:efi_application /entry:efi_main /nodefaultlib`，只链一个 obj，不依赖 gnu-efi ✓；kernel 用 gcc + GNU ld + nasm
- 镜像 strings 里搜 EDK2/TianoCore/OVMF/Copyright：0 命中；OVMF 只在 run-m1-uefi.sh 里通过 -drive pflash 使用 ✓
- 已有文件都没有 SPDX（报告如实写了「部分满足」）

结论：**部分符合**（与报告一致：项目许可证未定，已有文件无 SPDX）。

## 7. 「默认镜像不含什么」核实（重点）
### 7a 环境变量与编译宏
- build-m1-default.sh 第 10–11 行 unset：KERNEL_CFLAGS_EXTRA USER_NASMFLAGS_EXTRA INITRD_MANIFEST UEFI_CFLAGS_EXTRA UEFI_KERNEL_ELF UEFI_INITRD_BIN UEFI_LOADER_SOURCE UEFI_BUILD_DIR
- product/tools 里所有读取的变量：build-bios.sh 读 INITRD_MANIFEST、KERNEL_CFLAGS_EXTRA、USER_NASMFLAGS_EXTRA；build-uefi.sh 读 UEFI_CFLAGS_EXTRA、UEFI_KERNEL_ELF、UEFI_INITRD_BIN、UEFI_BUILD_DIR、UEFI_LOADER_SOURCE；build-m1-default.sh 读 AFOS_M1_OUT（只影响输出目录）；py 工具不读环境。**脚本自己读的变量都被覆盖了**
- **漏网**：工具自己会读的环境变量没清掉。实测 `NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY bash product/tools/build-m1-default.sh`，exit=0，kernel.elf 变成 a40151db…、initrd.bin 变成 7655c944…、img 变成 d00e1378…，而 build-info.txt 仍然写 `build=f66c5d7`，没有 dirty 标记（logs/item7a-inject.txt）。gcc 的 CPATH/C_INCLUDE_PATH/GCC_EXEC_PREFIX、clang 的 CCC_OVERRIDE_OPTIONS 等同理（没逐一实测）。`CFLAGS=-DX` 对构建没影响（img 不变）
- 实际编译命令行：`bash -x` 跑 build-bios.sh 和 build-uefi.sh，抓到 23 条 gcc/nasm/ld/python3/clang/lld-link 命令，含 `-D` 的 0 条（logs/item7a-trace-bios.txt）

### 7b 符号与调用点（logs/kernel-nm.txt、item7b-syms.txt）
- 冻结 kernel.elf 一共 306 个符号；agent_os_process_* 20 个；schedule_into_next、isr_stub_timer、isr_syscall、kernel_syscall_handler、scheduler_halt、pic_mask_all 在；**pic_timer_init 不在** ✓
- kernel.c（1832 行）kernel_main 从 1393 行开始：1399 `pic_mask_all()`，1400 `interrupts_init()`，1401–1403 `#if defined(AGENT_OS_TEST_TIMER_PREEMPT) pic_timer_init()`；1506–1515 DIV0；**1517 `#if defined(AGENT_OS_TEST_RING3)` … 1825 `#else` 打印 RING3 ABI READY … 1827 `#endif`**，范围与报告一致 ✓
- 但「这些函数的调用点都在 1517–1827」不准确：schedule_into_next（1347）、scheduler_halt（1344/1348）、user_range_ok 里的 agent_os_process_current（约 497 行）在 syscall 处理路径里，没有条件宏。默认启动不会走到，因为没有 Ring 3 代码执行，内核也不发 int 0x80，但是 0x80 门是装好的（见 7c）
- 默认路径（1393–1830）里：没有 sti，没有 PIC 取消屏蔽（只有 pic_mask_all），没有 pic_timer_init，不调度，不走 IPC，不调 syscall。全仓库 product/kernel 搜 `\bsti\b` 0 命中
- **报告说法有误**：报告把「内核态 GDT/TSS 初始化（load_kernel_gdt、tss_init）」列为「默认启动里真的跑到了」。实际上 tss_init() 唯一的调用点在 1518 行，在 `#if defined(AGENT_OS_TEST_RING3)` 里面；load_kernel_gdt 只在 tss_init 里调用（363 行）。默认启动**不执行** GDT/TSS 初始化

### 7c IDT
- interrupts_init()（kernel.c 约 449–481 行）：256 个向量都装成 0x8E（ring0 中断门），处理函数取 isr_stub_table；interrupts.asm 里这张表的第 32 项是 `isr_stub_timer`（push 32 → isr_common），33–255 是 isr_stub_spurious；**vector 0x80 另装成 isr_syscall，type_attr=0xEE（DPL3）**。默认镜像里这两个门都装了
- vector 32 进入 kernel_exception_handler：只有 AGENT_OS_TEST_TIMER_PREEMPT 下才处理来自 Ring 3 的 32（1358–1364），默认镜像里一律打印 `EXCEPTION observed` 走异常路径（PIC 全屏蔽、IF=0，实际不会到达）
- IF：kernel_entry.asm 第 15 行 `cli` 后 call kernel_main；默认路径没有 sti；末尾 1829–1831 `for(;;) hlt`（只有 hlt，前面没 cli，但 IF 早在入口就清了），等于 `cli; hlt` 状态，不是 `sti; hlt`
- 报告「IDT 256 个异常向量」的写法不准：256 个里只有 0–31 是异常，32 是定时器桩，0x80 是 DPL3 syscall 门。报告「不包含」清单里没提 0x80 门在默认镜像里装好、能从 Ring 3 进入

### 7d initrd（logs/item7d.txt）
- 来源：build-bios.sh 用 product/services/examples/echo.manifest.json 和 user_program.asm 链出的 user.elf，经 make-initrd.py 打包
- 解析 initrd.bin（8999 字节）：header magic AGOSINIT v2；manifest 343 字节（service_id=echo，entrypoint=/sbin/echo-service.elf，capabilities ipc:supervisor send/receive，restart on-fault…）；1 个 service，偏移 399，长 8600，ELF
- 这个 ELF 就是测试用户程序 user_program.asm（无宏编译），strings 里有 `USER RING3 OK`、`SUPERVISOR FAULT …`、`IPC CANCEL …`、`SERVICE #PF CRASH INJECTED` 等测试输出串（数据段不受 %ifdef 控制）；同一个 user.elf 还被 incbin 进了 kernel.elf（user_image_start/end 符号，0x10dd94–0x10ff2c），user_entry 在 0x186000
- 默认启动时，loader 会把 INITRD.BIN 读进内存（串口 `INITRD OK`），但解析、装载、运行都在 RING3 宏里，不会运行 ✓。报告没有写明「镜像里包含测试用户程序（initrd 一份，kernel.elf 内嵌一份）」

### 7e 测试宏数量（logs/item7e.txt）
- `rg -c 'AGENT_OS_TEST_' product/kernel`：user_program.asm 64、uefi_loader.c 1、kernel.c 38
- `grep -rhoE 'AGENT_OS_TEST_[A-Z0-9_]+' product/kernel | sort -u | wc -l` = **46**，报告写 45，**不准确**（只在 .c/.h 里的是 19 个；整个 product 也是 46 个）

### 7f 默认 kernel.elf 的测试输出串（logs/item7f.txt）
| 串 | kernel.elf 次数 | initrd 次数 |
|---|---|---|
| TIMER PREEMPT | 0 | 0 |
| SCHEDULER | 2（`SCHEDULER idle - all tasks exited`、`SCHEDULER switched task`，在未加宏的 syscall 路径里） | 0 |
| G3  | 9 | 4 |
| G4  | 2 | 1 |
| G6  | 8（virtio 探测，默认代码） | 0 |
| G7  | 2（`G7 virtio block FLUSH SUPPORTED/UNSUPPORTED`，默认探测代码） | 0 |
| G8/G9/G10/TOKEN/JOURNAL/PAUSE/DIV0 | 0 | 0 |
| SYSCALL | 78 | 0 |
| USER RING3 OK、SUPERVISOR FAULT…、IPC CANCEL… | 有（来自内嵌的 user.elf） | 有 |
| G2 CR3 switch OK | 1（kernel.c 235 行未加宏，和 1822 行的同一串合并了） | 0 |
- 与报告的对照：报告说「定时器抢占」「G7 token/pause/journal/block」「G8/G9/G10」「div0」不包含，字符串层面与此一致；`G7 ` 两条是 virtio 探测里的标志打印，不是 G7 功能。但报告写「Supervisor 和 initrd 服务的启动」「G4」不包含，而**测试用户程序和它的测试输出串在镜像里**（不执行）；SCHEDULER/SYSCALL 串在默认 syscall 路径里

结论：**部分符合**。默认启动路径确实不开中断、不调度、不进 Ring 3（核到了）。不准确的地方：tss_init/load_kernel_gdt 的说法写错；宏数量是 46 不是 45；没写 0x80 DPL3 门和测试用户程序在镜像里；「调用点都在 1517–1827」对 syscall 路径不成立；unset 清单没挡住 NASMENV 这类工具自己读的环境变量注入，而且注入后 build-info 仍显示干净版本。

## 8. 回归 95 项
- 读了 <评估目录>/m1-t0/regression-run-each.sh：按 run-all.sh 里出现的顺序去重，逐项 `timeout 300`，cd 写死 clean-d/src，日志写 ../../logs/regression/；没设 AFOS_OVMF_VERSION
- round5 基线（git show origin/main:linux-agent-os/research/round5-run-each-summary.tsv，95 行）与冻结 logs/regression-summary.tsv（95 行）：测试名完全相同，**rc 没有一项不同**
- 失败的 9 项：g6-virtio-net-rx、g6-virtio-net-tx、g6-virtio-block、g7-native-block-write/flush/read、g7-native-journal-recovery、g7-native-journal-commit-recovery（就是任务清单里那 8 项）加上 g2-irq ✓
- 冻结回归日志里 0 字节的 7 个：g7-native-block-write、g2-irq、g6-virtio-block、g6-virtio-net-tx、g7-native-block-flush、g7-native-block-read、g6-virtio-net-rx。原因是这些测试的 QEMU 日志写在 build/ 下（比如 g2-irq 写 build/bios/qemu-g2-irq.log），在 `grep -Fq 'TIMER PREEMPT switched task'` 这类检查失败时静默 exit 1；送审方没有把 build/ 下的失败日志收进冻结目录
- 我的重跑：<临时目录>/regress.sh（只改了 cd 到 <临时目录>/a 和日志路径），`nohup timeout 5400 …`，PID 909665，16:19:32 开始，16:25:31 跑完 95/95。结果：**86 通过 / 9 失败，失败名单与冻结、round5 完全相同**；同样 7 个 0 字节日志。我另外把 build/ 下 63 个 qemu-*.log 收进了 logs/regression-buildlogs/（g2-irq 的串口末尾是 `SCHEDULER idle - all tasks exited`，没有 `TIMER PREEMPT switched task`，没有 EXCEPTION）。回归后 a 里的 build/m1-default/afos-m1-uefi.img 仍是 15ec6371…，git status 干净
- 进度：<临时目录>/regression-progress.txt；汇总：logs/regression-summary.tsv；逐项日志：logs/regression/

结论：**符合**（无新增失败）。次要（门槛 4）：冻结目录里 9 项失败中有 7 项的日志是空文件，实际失败输出没有冻结。

## 9. 未测项是否如实，镜像格式
- 报告「没验证的」覆盖了：KVM ✓、其他 OVMF（含 Secure Boot 版）✓、真机固件 ✓、其他 QEMU ✓、其他宿主工具链 ✓、superfloppy 无分区表 ✓
- 镜像：`file` → FAT (16 bit)，OEM "AFOS    "，label "AFOSM1ESP  "，32768 扇区，每簇 4 扇区，2 个 FAT，每 FAT 32 扇区，根目录 512 项，介质 0xf8；大小 16777216 字节 = 16 MiB；od：0x1BE–0x1FD 全 0（没有分区表），0x1FE = 55 AA；根目录只有 AGENTOS、EFI 两个目录。盒子上没有 xxd，用 od 代替（logs/item9.txt）
- 「逐条自查」各项的依据：
  - T1 满足：核到（第 1、7a 节）；但 unset 挡不住工具读的环境变量（见缺陷）
  - T2 满足：核到（第 2 节）
  - T3 满足：核到（111 个都是 100755，OVMF 变量，g5-stage0 新检出直接跑 PASS）
  - T4 满足：核到 134 条、只读；只读强度见第 3 节
  - T5 满足：**夸大**。清单有错（tss_init/GDT）、有遗漏（0x80 门、测试用户程序在镜像里），宏数量也不对
  - T6 满足：核到（回归名单不变，默认 QEMU 命令没有 virtio 设备）
  - 门槛 2「构建日志含命令、退出码」：靠仓库外的包装，仓库脚本自己不打印
  - 门槛 4「失败日志都保留」：回归失败日志 7 个是空文件，算部分满足
  - 门槛 3、10：核到；门槛 7：核到；门槛 8：核到（ovmf_version 是手填）

结论：未测项列表**符合**；自查表 T5 和门槛 4 **有夸大**。

## 10. 断网构建与硬编码路径
- `unshare -rn` 新网络命名空间（只有 lo、DOWN，DNS 不通）里跑 build-m1-default.sh，exit=0，img 仍是 15ec6371… ✓（logs/item10.txt）
- `rg -n '<工作区>/' product`：0 命中；`git grep '<工作区>/' f66c5d7`：0 命中 ✓。新脚本里的绝对路径只有 run-m1-uefi.sh 14/15 行的 OVMF 默认值 /usr/share/OVMF/*_4M.fd（可以用环境变量覆盖）
- 盒子专属路径只在仓库外：env.sh、regression-run-each.sh（写死 clean-d/src）、报告的复跑方法

结论：**符合**。

## 发现的缺陷与疑点
1. **重要**：功能清单（T-M1-0 第 5 条）把 GDT/TSS 初始化算成「默认启动里真的跑到了」，实际 tss_init() 只在 `#if defined(AGENT_OS_TEST_RING3)`（kernel.c:1517–1518）里调用，load_kernel_gdt 只在 tss_init 里（kernel.c:363）。这正是第 5 条禁止的「把测试宏里才有的东西算进去」。证据：第 7b 节
2. **重要**：默认构建能被工具自己读的环境变量注入测试宏，产物悄悄改变，版本行不变。`NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY` 改变了 kernel.elf/initrd/img，build-info 仍是 `build=f66c5d7`。证据：logs/item7a-inject.txt。建议脚本里清掉 NASMENV、CPATH、C_INCLUDE_PATH、GCC_EXEC_PREFIX、COMPILER_PATH、CCC_OVERRIDE_OPTIONS 等，或用 `env -i` 加白名单
3. **次要/重要**：功能清单漏了几样东西：默认镜像装了 DPL3 的 int 0x80 门（kernel.c 约 465–475 行）和 vector 32 桩；镜像里有测试用户程序 user_program.asm（initrd 一份，kernel.elf 内嵌一份），带测试输出串。都不执行，但「镜像里有什么」应该如实写。证据：第 7c、7d、7f 节
4. **次要**：测试宏数量是 46 不是 45（logs/item7e.txt）
5. **次要**：「调用点都在 1517–1827」不严谨：schedule_into_next 等在 syscall 路径里没有宏保护（kernel.c:1344–1350），只靠没有 Ring 3 执行来保证不触达
6. **次要（门槛 4）**：冻结回归日志里 9 项失败中 7 项是 0 字节，失败的串口输出（在 build/ 下）没有冻结。证据：第 8 节
7. **次要**：b4496fc 改了 linux-agent-os/prototypes/proto1-landlock/env.sh 的模式（644→755），和 T-M1-0 无关，和报告「只改了产品代码」不一致
8. **次要**：冻结的只读只是 chmod a-w，属主 构建用户 能随时改回来，父目录可写
9. **次要**：日志头里 ovmf_version 是环境变量手填（不设就是 unknown），kernel_version 取自镜像旁的 build-info.txt，不是从镜像本身读的；构建日志的 cmd/exit 行由仓库外包装产生
10. **次要**：工具链依赖盒子的 env.sh（/usr/bin 里没有 nasm/clang/lld-link），build-m1-default.sh 缺工具时直接报 127，README 只列了工具名

## 我没来得及核的
- gcc 的 CPATH/GCC_EXEC_PREFIX、clang 的 CCC_OVERRIDE_OPTIONS 等注入没逐一实测，只实测了 NASMENV
- 没有对 kernel.elf 反汇编，逐条确认默认路径的机器码里没有 sti（只查了源码和 asm，盒子 rg 0 命中）
- UEFI loader 在 ExitBootServices 前后对 IF 的处理没细读（内核入口有 cli，结论不受影响）
- 串口里 UEFI 各标记都打了两次，原因没查（可能 loader 同时写了 ConOut 和串口）
- 冻结 logs 里 build-001..003、run-001 失败尝试的内容没逐份细读，只核了它们在清单里、哈希对得上
- KVM、其他 OVMF、真机：盒子上条件不具备，没测

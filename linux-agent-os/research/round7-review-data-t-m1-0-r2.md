> 入库说明：验收审查 t-m1-0-r2 的数据记录原文，只做路径、构建用户名、账号的替换（见 `research/round7-stage-reviews.md` 第二节）。日志文件没有入库，哈希见 `research/round7-review-logs-sha256.txt`。

# T-M1-0 修订版（r2）复核数据

- 时间：2026-11-02 16:42–16:58（UTC+8）
- 对象：work 分支 t-m1-0-r2，HEAD 428d604dab0b…（a61e8f8 + 428d604，父提交 f66c5d7）；冻结目录 freeze/afos-m1-t0-0.1.0-dev-428d604；报告 T-M1-0-REPORT.md（r2）
- 我的检出：<临时目录>/r2/{a,b,inj,pp,reg}；日志在 <T-M1-0复核目录>/r2/logs/
- 没有修改被审源码，也没有改 <评估目录>/m1-t0 下的任何文件
- 环境：`. <评估目录>/env.sh`；AFOS_OVMF_CODE/VARS=$OVMF_DIR/OVMF_{CODE,VARS}_4M.fd

## A. 功能清单更正（logs/A-source.txt、nm-428d604.txt）
命令：`git diff --stat f66c5d7 428d604 -- product/kernel/src product/kernel/arch/x86_64/*.asm`；`sed -n` 取行号；`nm`；`grep -rhoE … | sort -u | wc -l`；`strings`
- 内核源码与 asm 在 f66c5d7 和 428d604 之间没有变化（diff 为空），所以行号与上一轮相同；这次改动的只有 uefi_loader.c
- GDT/TSS：kernel.c:1517 `#if defined(AGENT_OS_TEST_RING3)`，1518 `tss_init();`；363 行 `load_kernel_gdt(&gdtr);` 在 tss_init 函数体内。符号表里 tss_init（0x1012d2）和 load_kernel_gdt（0x100014）都有，属于编进镜像但默认不调用。报告已改为「默认不执行」✓
- 0x80：kernel.c 464–475 行 `idt[0x80]` 装 isr_syscall，type_attr=0xEE（DPL3）✓；vector 32 是 isr_stub_timer（0x10013c）✓；pic_timer_init 不在符号表 ✓
- 内嵌测试程序：user_image_start 0x10dd94，user_image_end 0x10ff2c ✓；initrd.bin 里有 `USER RING3 OK`、`USER RING3 TASK2 OK`、`FAULT RECOVERY TASK2 OK` ✓；kernel.elf 里含 SYSCALL 的串 78 条 ✓
- 测试宏 46 个 ✓
- SYS_EXIT：1340–1351 行没有宏保护，调用 agent_os_process_clean_exit、scheduler_halt、schedule_into_next ✓（报告写 1340–1350，差 1 行，不影响）
- 自查第 5 条、门槛 4 都已改为「部分满足」✓
- 小出入：报告写默认启动最后「进入 `cli; hlt` 循环」。源码 1829–1831 行只有 `hlt`，IF 是在 kernel_entry 入口的 cli 清掉的，效果等同但写法不准确。报告说第一条 cli 在「_start 入口」，反汇编里这个符号是 `__kernel_text_start`（kernel_entry）
- 结论：**符合**（只剩措辞小出入）

## B. 环境变量注入（logs/B-inject.txt、B-inject2.txt、B-exitcodes.txt）
命令：在 <临时目录>/r2/inj 里依次 `env VAR=… bash product/tools/build-m1-default.sh`，每次记录 img/EFI/kernel/initrd/build-info 的 sha256 前 12 位
- 基线：img 658126ea8ce0、EFI e0d13256ee46、kernel 5aca02592b05、initrd 0da1036ff427、build-info 9edc447640e4
- 以下每个变量单独设置，构建结果**全部等于基线**（包括 build-info）：
  - NASMENV=-DAGENT_OS_TEST_RING3=1、CPATH 和 C_INCLUDE_PATH（指向含 `#define AGENT_OS_TEST_RING3 1` 加 include_next 的伪 stdint.h/stddef.h）、CCC_OVERRIDE_OPTIONS=+-DAGENT_OS_TEST_FORCE_MAP_KEY_FAILURE、CFLAGS、LDFLAGS=-s、KERNEL_CFLAGS_EXTRA、AGENT_OS_TEST_RING3=1
  - GCC_EXEC_PREFIX=/nonexistent/、COMPILER_PATH、LIBRARY_PATH、CPLUS_INCLUDE_PATH、OBJC_INCLUDE_PATH、SOURCE_DATE_EPOCH=1、TMPDIR=/nonexistent、PYTHONPATH、PYTHONHASHSEED=123、LD_PRELOAD=自制 .so
  - HOME 指向带 core.abbrev=12 的 .gitconfig
  - 补做的 NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY（这个宏确实能改变 asm 产物）
- 实验设计的说明：AGENT_OS_TEST_RING3 只在 kernel.c 里起作用，user_program.asm 没有 RING3 分支，所以用 NASMENV 注入 RING3 本来就不会改变产物。上一轮能改产物的是 FAULT_RECOVERY，我补做了这一项
- **重入保护能被绕过**：脚本靠 `AFOS_M1_SANITIZED != 1` 决定要不要 `exec env -i`。调用方自己先设 `AFOS_M1_SANITIZED=1`，就会跳过净化：
  - `AFOS_M1_SANITIZED=1 NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY`：img 变成 600ff988bc1f，kernel 220b5a97dd68，initrd 7655c944d303；版本行仍是 `build=428d604`；build-info 的 `test_macro_hooks` 能显示 NASMENV 的值，但 `build_env=env -i (kept: …)` 这一行照样写「env -i」，保留变量列表里多出了整个调用方环境
  - `AFOS_M1_SANITIZED=1 CPATH=…`：img 变成 ada04f42c8e6，kernel b95a4cfc6cac。test_macro_hooks 里**不显示** CPATH，只有 build-env.txt 和 build_env 的变量名列表里能看到
  - `AFOS_M1_SANITIZED=1 KERNEL_CFLAGS_EXTRA=…`：产物不变（脚本后面还有 unset），只有 build-info 变了
  - 重入本身不会死循环：exec 时设了 AFOS_M1_SANITIZED=1，第二次进来直接往下走。但这个标志不能防外部伪造
- 假 nasm（PATH 前放一个包装脚本，调真 nasm 时加 -DAGENT_OS_TEST_FAULT_RECOVERY）：img 变成 600ff988bc1f，kernel、initrd 都变了，版本行不变。build-info 里只有 `tool_nasm=<临时目录>/r2/fakebin/nasm` 能看出来，与自述「只做到可见」一致
- env -i 保留的变量：PATH、HOME、LC_ALL=C、TZ=UTC、AFOS_M1_OUT（可选）、AFOS_M1_SANITIZED
  - PATH 能决定用哪套工具（见上）
  - HOME：实测改 .gitconfig 的 abbrev 不影响产物，因为脚本用的是 `--short=7`；其他 git 配置没有穷举
  - build-env.txt 会记录 PATH 原值，我这边 PATH 不同，所以 build-env.txt 的哈希（a42483…）≠ 冻结版（ac7798…）。镜像哈希不受影响，但冻结的 artifacts/SHA256SUMS 别人很难对上
- build-info 新字段是真算的还是写死的：
  - `tool_*` 是 `command -v` 实时取的，真实
  - `test_macro_hooks` 是在 env -i 和 unset 之后才打印，正常路径下必然全空，**不构成检查**。它只在绕过路径下能显示值，而且不覆盖 CPATH、CCC_OVERRIDE_OPTIONS 等
  - `build_env=env -i` 是字面量，绕过时也照写
- 退出码：
  - `bash -x build` 退出 0；但 exec env -i 之后的内层 bash 没有 -x，只跟踪到 3 行，调试作用丢失
  - 缺 nasm 时：普通方式、`bash -x`、绕过方式退出码都是 3
  - 在 `set -e` 的父 shell 里调用，成功时是 0
  - AFOS_M1_OUT=/proc/forbidden 时退出 1
  - `AFOS_QEMU=/nonexistent/qemu` 时 run 脚本退出 3
  - 没有看到退出码被误判
- 结论：**部分符合**。正常调用下注入都被挡住了，实测 20 个变量。但只要设一个 AFOS_M1_SANITIZED=1 就能绕过净化，绕过后 build-info 仍然声称 env -i，test_macro_hooks 也不是真检查

## C. 次要项
1. 失败日志（logs/C1-outputs.txt、C1-content.txt）：冻结 logs/regression/ 里那 7 个 stdout 日志仍是 0 字节（报告说按规定保留），真实输出放在 test-outputs/bios/qemu-<名>.log 和 .stderr。test-outputs 共 123 个文件，没有 0 字节的。内容核对：g2-irq、g6-virtio-net-rx/tx、g6-virtio-block 的串口都以 `SCHEDULER idle - all tasks exited` 结束，加上一行 SIGTERM；g7-native-block-write/flush/read 是 `SCHEDULER block device capability unavailable`。这与测试失败原因对得上（没有 TIMER PREEMPT、virtio 设备不可用）。**符合**
2. env.sh：`git diff --stat 0066bee t-m1-0-r2` 里不再有 linux-agent-os 的文件（105 个文件全在 product/）；f66c5d7→428d604 是 100755→100644。**符合**
3. ovmf_version（logs/C3-ovmf.txt）：AFOS_OVMF_VERSION 已经删了，设成 FAKE-1.0 也被忽略 ✓。新字段有两个：
   - ovmf_package：读固件文件旁边 `../doc/ovmf/changelog.Debian.gz` 的第一行。我把同一份固件复制到 <临时目录>/r2/fw/share/OVMF/，旁边放一个伪造的 changelog，日志头就显示 `edk2 9999.99-FAKE`；去掉 changelog 就是 unknown。所以这是**文件旁的元数据，能伪造，不是实测**
   - ovmf_runtime：真实来自启动时 SystemTable，值是 `vendor=Debian distribution of EDK II revision=0x00010000`。但 revision 0x00010000 是通用值，区分不了 edk2 版本
   - 盒子上没有第二种 OVMF 可换（/usr/share/OVMF 不存在）
   - 结论：**部分符合**。报告称「OVMF 版本是实测的」，这有夸大
4. 缺工具：`env PATH=/usr/bin:/bin` 下构建，提示 `missing tool(s): nasm clang lld-link file`，退出码 3 ✓（检查清单里有 sha256sum，提示文字里没写，属于小瑕疵）。**符合**
5. 冻结权限：dr-xr-xr-x，属主仍是 构建用户；没有找到可写文件；盒子上没有 lsattr。BASELINE.txt 写明了「chmod a-w 属主可随时撤销，完整性以 SHA256SUMS 为准」。只是写明了限制，没有修。SHA256SUMS 自身的哈希写在冻结目录内的报告副本里，没有放到目录外的独立位置。**已如实披露，问题未消除**

## D. 复现（logs/D-build.txt、D-freeze.txt、D-vs-old.txt、runs/）
- a、b 两个克隆：HEAD 都是 428d604dab0b…，分支 t-m1-0-r2，porcelain 0 行；原样构建都是 exit 0（约 0.5 s）；a 原地重建 exit 0
- 哈希：img 658126ea…d32b、BOOTX64.EFI e0d13256…4e96、kernel.elf 5aca0259…c011、initrd.bin 0da1036f…1c46、build-info.txt 9edc4476…，a、b、冻结 artifacts/ 五者一致；build-env.txt 与冻结版不同，原因是 PATH（见 B）
- 断网：`unshare -rn` 下 b 重建 exit 0，img 仍是 658126ea… ✓
- 启动：a 两次、b 一次，都 exit 0、qemu_exit 124。三份串口 sha256 都是 d3ae60a2…4acb，与冻结 logs/boot-clean-a/run-001、run-002、boot-clean-b/run-001 一致
- 串口内容：第 5 行 `AGENTOS UEFI FIRMWARE vendor=Debian distribution of EDK II revision=0x00010000`；EXIT BOOT SERVICES 两次；`KERNEL C OK`；`AFOS-M1 version=0.1.0-dev build=428d604` ✓
- 日志头字段：accel、qemu_version、ovmf_code/vars 路径和 sha256、ovmf_package、image sha256、kernel_build_info、run_seconds、qemu_cmd、qemu_exit、ovmf_runtime、kernel_version_serial，齐全 ✓（ovmf_package 的问题见 C3）
- 冻结：`sha256sum -c` exit 0，0 项失败；255 条 ✓；SHA256SUMS 自身 65dcb20792c4…3ef13 ✓；没有清单外的文件，也没有清单有但缺失的文件；没有可写文件和符号链接；报告副本与 T-M1-0-REPORT.md 一致；scripts/ 里三个产品脚本与仓库一致；patches/ 与 patches-r2/ 一致
- 补丁：在 f66c5d7 上依次 `git apply` 0001、0002，write-tree a5ab8cce… = 428d604^{tree} ✓
- 旧冻结：SHA256SUMS 仍是 dfa2efb8…，`-c` 0 项失败，没有被动过 ✓
- 与旧冻结比：
  - initrd.bin 相同
  - kernel.elf 只差 7 字节，位于文件偏移 0xEB09–0xEB0F。版本串在 0xEAE8，加上前缀长度 0x21 正好到 0xEB09，也就是 `f66c5d7`→`428d604` 这 7 个字符
  - `.text`、`.data`、`.bss` 字节一致，只有 `.rodata` 不同；`objdump -d` 全文 diff 0 行 ✓
  - BOOTX64.EFI 变了：.text 从 0x168c 变成 0x182c，.rdata 从 0x1b4 变成 0x1e4。这来自新增的 report_firmware，是 loader 的功能改动，不只是版本串
  - img 随之改变（6483 字节不同）
- 结论：**符合**。说明一点：「镜像只有版本行相关的变化」不成立，loader 有功能改动，但这个改动本身是送审内容

## E. 改动面（logs/E.txt）
- `git diff --stat f66c5d7 428d604` 共 5 个文件：
  - env.sh：只改模式，回退
  - uefi_loader.c +24：report_firmware 函数，加一处调用
  - build-m1-default.sh +27/-2：env -i 重入、缺工具检查、build-env.txt、build-info 新字段、SPDX
  - make-esp-image.py：只改 SPDX
  - run-m1-uefi.sh +21/-3：QEMU 检查、ovmf_package、ovmf_runtime、kernel_version_serial、删 AFOS_OVMF_VERSION、SPDX
- 没有与上述无关的改动；测试脚本和断言没有改
- 作者和提交者都是 afos-codex <afos-codex@localhost>
- 名字检查：用 rg 搜项目负责人的中文全名、名字和拼音，范围是 diff、冻结目录全部文件、428d604 全树、报告，0 命中；/home 非 构建用户 路径 0；product 下 `<工作区>/` 0
- SPDX：build-m1-default.sh、run-m1-uefi.sh、make-esp-image.py 第 2 行都是 `# SPDX-License-Identifier: MIT`；冻结 scripts/ 下的 clean-runs.sh、inject.sh、regression-run-each-r2.sh 也标了 MIT（这几个不在仓库里）
- 仓库仍然没有 LICENSE/COPYING，只有 product/docs/LICENSE_MATRIX.md，里面仍写着「项目自身的公开许可证在首次发布前由维护者最终选择」
- MIT 由谁决定、依据是什么：提交信息、报告、BASELINE、origin/main 规格里**都没有找到记录**（规格里出现 MIT 的地方只有 round4 对 BearSSL 的描述）。也就是说，没有书面的决定来源
- 门槛 6 的状态：三个新文件有 SPDX；仓库没有许可证文本；已有文件没有 SPDX；与 LICENSE_MATRIX「待定」的说法不一致
- 结论：改动面**符合**；门槛 6 **无法判断是否已获授权**（只记录事实）

## F. 反汇编（logs/F-disasm.txt）
命令：`objdump -d -M intel`，只统计助记符，awk 记录所在函数
- kernel.elf：sti 0 条；cli 4 条，分别在 `__kernel_text_start` 0x100000、`enter_user_mode` 0x10002e、`scheduler_halt` 0x100fd1、`kernel_exception_handler` 0x10408a，与报告的 4 个地址一致 ✓。.user_text 里 sti/cli 都是 0。原始字节里 0xFB 有 43 个、0xFA 有 208 个，都是误报来源，所以不用字节计数
- BOOTX64.EFI：sti 0、cli 0 ✓（原始 0xFB 3 个、0xFA 1 个，都不是指令）
- 只反汇编了可执行节；.rodata 里内嵌的 user.elf 没有当代码反汇编，它对应的 .user_text 已经查过
- 打印两次的原因：uefi_loader.c:331–335 `report()` 先 serial_puts（直接写 0x3F8 端口，284–289 行），再 console_puts 写 ConOut；新加的 report_firmware 只调 serial_puts，在串口里只出现一次（第 5 行），这和「OVMF 的 ConOut 也输出到串口」的解释一致 ✓。不过我没有直接验证 OVMF 的 ConOut 重定向配置，这一点是推断
- 结论：**符合**

## G. 回归（logs/regression-summary.tsv、G-compare.txt、regression/、regression-buildlogs/）
- 复用了上一轮的 <临时目录>/regress.sh，只把目录改成 <临时目录>/r2/reg（新克隆，**没有先做默认构建**）。后台 `nohup timeout 5400`，PID 972466，16:46:53 开始，16:52:51 跑完 95/95，进程已退出
- 结果：**85 通过 / 10 失败**。比 round5、旧冻结、r2 冻结多 1 项失败：**g6-mmio-map-test.sh**，日志是 `/usr/bin/ld: cannot open output file …/build/g6-mmio-map-host: No such file or directory`。这个测试是 run-all 里的第一项，它往 build/ 里写文件却不 `mkdir -p`，在全新克隆里必然失败。等后面的测试建出 build/ 后单独重跑，PASS
- 其余 9 项失败与之前相同：8 项 virtio/g7 加 g2-irq。stdout 0 字节的同样是那 7 个；它们在 build/ 下的真实输出我收进了 regression-buildlogs/（117 个文件）
- 送审方的 r2 回归是「先做默认构建再跑」（报告原话），所以 build/ 已经存在，掩盖了这个问题
- 结论：**基本符合**（扣除 mmio-map 后与基线一致）。mmio-map 的失败属于「干净目录原样复跑」问题，送审方和上一轮的我都没发现（上一轮我的克隆里有默认构建留下的 build/）

## 仍存在的缺陷与疑点
1. **重要**：只要调用方设 `AFOS_M1_SANITIZED=1` 就能跳过 env -i 净化，NASMENV、CPATH 注入随即生效，产物改变而版本行不变，build-info 还写着 `build_env=env -i`。证据：logs/B-inject.txt、B-inject2.txt（BYPASS_* 各行）
2. **重要**：PATH 里换成假工具能改变产物，只在 build-info 的 tool_* 里留下痕迹，版本行和镜像本身都看不出来。送审方自述了这一点。证据：B-inject2.txt FAKE_NASM_FR
3. **重要**（干净目录复跑，T-M1-0 第 3 条的精神）：g6-mmio-map-test.sh 在没有 build/ 的新克隆里失败（缺 mkdir）。证据：logs/regression/g6-mmio-map-test.sh.log、G-compare.txt
4. **次要**：build-info 的 test_macro_hooks 在正常路径下必然为空，不是真检查；它不覆盖 CPATH 等变量；build_env 一行是字面量。证据：build-m1-default.sh 的 build-info 段
5. **次要**：ovmf_package 是固件旁 changelog 的文本，能伪造；ovmf_runtime 的 revision 是通用值，区分不了版本。报告说「OVMF 版本实测」有夸大。证据：logs/C3-ovmf.txt
6. **次要**：build-env.txt 记录 PATH 原值，所以不同 shell 下 artifacts/SHA256SUMS 对不上（build-env.txt 哈希 a42483… 与冻结的 ac7798… 不同），复现者容易误以为不一致
7. **次要**：MIT 许可证的选择没有找到决定记录；仓库没有 LICENSE；与 LICENSE_MATRIX「待定」矛盾。证据：logs/E.txt
8. **次要**：冻结仍然只靠 chmod，SHA256SUMS 自身的哈希只记在冻结目录内的报告副本里（已披露）
9. **次要**：`bash -x` 跟不进 env -i 后的内层执行；报告里 `cli; hlt`、「_start」的措辞不准确；缺工具提示没写 sha256sum

## 没来得及核的
- 只换了 OVMF 文件的位置（字节相同），没有一份真正不同版本的 OVMF 可测
- HOME 下其他 git 配置（例如影响 status 的设置）对 `-dirty` 判定的影响没有穷举
- run-m1-uefi.sh 在 KVM 下的行为没测（没有权限）
- 冻结 test-outputs 里其余 116 个日志只抽查了 7 个失败项
- OVMF 把 ConOut 重定向到串口的配置没有直接查证

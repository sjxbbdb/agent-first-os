> 入库说明：验收审查 t-m1-0-r3 的数据记录原文，只做路径、构建用户名、账号的替换（见 `research/round7-stage-reviews.md` 第二节）。日志文件没有入库，哈希见 `research/round7-review-logs-sha256.txt`。

# T-M1-0 第三版（r3）复核数据

- 时间：2026-11-02 17:32–17:55（UTC+8）
- 对象：work 分支 t-m1-0-r3，HEAD fe3e3cd7162c…。实际是两个提交：1acaf2e（构建净化重做）和 fe3e3cd（4 个测试补 mkdir），都基于 428d604，作者/提交者都是 afos-codex <afos-codex@localhost>
- 冻结目录：freeze/afos-m1-t0-0.1.0-dev-fe3e3cd；报告：T-M1-0-REPORT.md（r3）
- 我的检出：<临时目录>/r3/{a,b,g,reg,each/*,pre/*}；日志：<T-M1-0复核目录>/r3/logs/
- 没有修改被审源码，也没有改 <评估目录>/m1-t0 下的任何文件。工具链只复制到 <临时目录>/r3/envcopy*，原目录 <评估目录>/env 没动
- 构建命令：`bash product/tools/build-m1-default.sh --toolchain-dir=<评估目录>/env/bin`（README 和报告都写明构建不需要先 source env.sh）

## A. 干净克隆构建（logs/A-build.txt）
- a、b 两个克隆：HEAD fe3e3cd，分支 t-m1-0-r3，porcelain 0 行，都没有 build/
- 原样构建：a exit 0（584 ms），b exit 0（567 ms），a 原地重建 exit 0
- b 在 `env -i PATH=/usr/bin:/bin` 起的另一个 shell 里用 `unshare -rn` 断网构建，exit 0
- 哈希：a、b、冻结 artifacts/ 三方一致，冻结 SHA256SUMS 里的对应项也一致：
  - img e2837a4f05ccbf117df0abfcb70feef9b5f5f41f81dedb601af6855bf929b933 ✓
  - BOOTX64.EFI e0d13256…
  - kernel.elf 8d0a9027…
  - initrd.bin 0da1036f…
  - build-info.txt 00a002c3…
  - build-env.txt 150d7158…
- build-env.txt 的内容：GIT_CONFIG_NOSYSTEM=1、HOME=/nonexistent、LC_ALL=C、PATH=/usr/bin:/bin:<评估目录>/env/bin、TZ=UTC。上一轮因调用方 PATH 不同导致的差异**消除了**
- 仍会变化的情况：PATH 和 toolchain_dirs 由 `--toolchain-dir` 的字符串决定。工具链装在别的路径（或写法不同，比如带 `..`、换成符号链接），build-env.txt 和 build-info.txt 就会不同，冻结 SHA256SUMS 里这两项会对不上；镜像、EFI、kernel、initrd 不受影响
- 结论：**符合**

## B. 注入实验（logs/B-inject.txt、B-inject2.txt、B-inject3.txt、B-gitconfig.txt）
有效性对照（不走默认构建脚本，直接跑 build-bios.sh）：
- 不设变量：kernel 8d0a9027
- `NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY=1`：kernel 337fcf42，initrd 7655c944，产物变了
- CPATH 指向会定义 RING3 的伪 stdint.h：kernel 6dc112f7，产物变了
- 所以这两个注入手段本身有效

经过外层脚本（每次都先 rm -rf build/m1-default）：以下每一项都是 exit 0，img、kernel、initrd、EFI、build-info 全等于基线，版本行 `build=fe3e3cd`：
- NASMENV=-DAGENT_OS_TEST_FAULT_RECOVERY=1、CPATH、C_INCLUDE_PATH、CCC_OVERRIDE_OPTIONS、GCC_EXEC_PREFIX、COMPILER_PATH、LIBRARY_PATH、LD_PRELOAD、CFLAGS、LDFLAGS、KERNEL_CFLAGS_EXTRA、USER_NASMFLAGS_EXTRA（FAULT_RECOVERY）、UEFI_CFLAGS_EXTRA、PYTHONPATH、PYTHONHASHSEED、SOURCE_DATE_EPOCH、TMPDIR
- 预设 AFOS_M1_SANITIZED=1 加 NASMENV
- 把内层用到的变量名直接设给外层（env_bad、expected_path、toolchain_dirs、system_dirs、repo_root、out_dir、tools），再加 NASMENV
- BASH_ENV（指向会 export NASMENV 的文件）、SHELLOPTS=xtrace、`BASH_FUNC_nasm%%` 导出函数、GIT_DIR=/nonexistent、HOME 下带 .gitconfig

例外：预设 AFOS_M1_OUT 时 exit 2，不产生产物（按设计拒绝）✓

直接调用内层：
- 不带参数：exit 2
- 带着当前脏环境：exit 4，列出所有不符的变量
- env -i、白名单值正确：exit 0，产物等于基线
- env -i 加 NASMENV：exit 4
- env -i 但 PATH 和参数不一致：exit 4
- env -i 带 OLDPWD=/x、_=/y：exit 0，产物等于基线。这几个变量的值不检查，实测对产物没有影响
- **env -i、白名单值都对，但 SYSTEM_DIRS 参数里自带假目录（`/tmp/…/fake1:/usr/bin:/bin`），PATH 跟着这样拼：exit 0，img 8c3623d9、kernel 337fcf42，版本行仍是 build=fe3e3cd**。也就是说，内层的「预期目录」完全由调用方传进来的参数决定，直接调内层时没有任何固定的锚点

白名单内的变量：
- HOME 由外层固定成 /nonexistent，调用方设的 HOME 不会生效（实测）
- 仓库自己的 .git/config 仍会被读取：
  - 设 `core.fsmonitor` 为任意命令，构建时这条命令**被执行了 2 次**（FSMONITOR_RAN 写了两行），exit 0，产物等于基线
  - 对 kernel.c 做 `git update-index --assume-unchanged` 后改源码：`git status` 0 行，构建得到 img 40af84bf，kernel.elf 里有改过的串，**版本行仍是 build=fe3e3cd，没有 -dirty**
  - 报告「没验证的」一节承认没有穷举 .git/config 的影响

假工具：
1. 假 nasm 放在调用方 PATH 前面：等于基线（外层不继承 PATH）✓
2. 把整个工具链目录复制一份，替换其中的 nasm，用 `--toolchain-dir` 指向这份拷贝：exit 0，img 8c3623d9、kernel 337fcf42、initrd 7655c944，版本行不变。tool_nasm_sha256 变成 f221f5b6…（原来是 f5b2b834…），tool_nasm_version 不变。下面几种写法也都能通过：
   - 符号链接目录（<临时目录>/r3/binlink）
   - `..` 前缀伪装（`<评估目录>/env/bin/../../../..<临时目录>/r3/envcopy/bin`）。build-info 记录的就是这个字符串，开头看起来像官方目录
   - 多给一个 `--toolchain-dir=/tmp/…/fake1` 放在正确目录前面，fake1 里只有一个假 nasm。fake1 优先，产物同样被改
   - 脚本根本没有「预期目录」的固定定义：传进来的目录就是预期目录，检查只确认工具解析结果落在这些目录里
   - 只复制 bin/ 不复制 lib/ 的拷贝跑不起来（exit 127，clang 找不到共享库），那是我实验的问题，不是脚本拦住的
3. 位置正确、内容是别的版本：clang 包装脚本对 `--version` 输出 `clang version 99.0.0 (fake)`，实际编译时加 -DAGENT_OS_TEST_FORCE_MAP_KEY_FAILURE。结果 exit 0，img 9ddc4162、EFI 4696b3f9。build-info 里 `tool_clang_version=clang version 99.0.0 (fake)`，`tool_clang_sha256` 也变了。可见，但版本是工具自报的，可以伪造。sha256 只覆盖 PATH 上那一个文件（符号链接会解析），不覆盖被包装的真实二进制和共享库；替换共享库的情况我没测

判断：
- 能挡住：所有调用方环境变量（包括标记变量、BASH_ENV、导出函数）、调用方 PATH
- 挡不住：`--toolchain-dir` 指向的任何目录里的工具（包括多给一个目录的情况）、直接调内层时自带的 SYSTEM_DIRS、仓库 .git/config 和 index 状态
- 被替换的工具能从 tool_* 的路径和 sha256 看出来，前提是有人拿去和一份已知正确的值比对；版本行不会变
- 「build-info 记录工具 sha256 与版本，防不住只做到可见」：对 `--toolchain-dir` 里的工具这个表述**如实**。但它没提到 .git 配置导致的 -dirty 隐藏和 fsmonitor 命令执行，也没说「预期目录」就是调用方随便给的目录
- 门槛 11 记「部分满足」，我认为合适

结论：**部分符合**（环境变量注入已经挡住；工具链和仓库状态防不住，表述基本如实）

## C. build-info（读 inner 脚本源码，对照我的实测）
- env_measured：内层用 `/usr/bin/env` 实测，快照来自 `env -0` 比对之后的同一个进程。实测时的预设变量（NASMENV、BASH_FUNC 等）都没有进入快照，因为外层 env -i 已经清掉，直接调内层时又会被 exit 4 拒绝 ✓
- toolchain_dirs、tool_*、tool_*_sha256、tool_*_version：都是运行时用 command -v、sha256sum、--version 取的，不是写死的（假 nasm、假 clang 实验里都跟着变了）✓
- 写死的部分只剩 `env_check=pass (...)`、`tool_dir_check=pass (...)`、`tool_note=...`。这几行只有在检查通过后才会走到，所以算真实结果，只是文案固定
- build_env、test_macro_hooks 已删除 ✓
- 结论：**符合**

## D. 无 build/ 回归（logs/regression-seq.tsv、regression-each.tsv、D-compare.txt、D-pre-fix.txt）
- 脚本 <临时目录>/r3/reg.sh：同 round5 的方式，`timeout 300`，按 run-all 的顺序
- 两种跑法，都不先做默认构建：
  - seq：一个全新克隆，没有 build/，95 项依次跑
  - each：每一项各用一个全新克隆，没有 build/
- 后台 `nohup timeout 5400`，PID 1153383；seq 17:35:27 开始，each 17:41:26 开始，17:47:29 全部结束，进程已退出
- 结果：seq **86 过 / 9 败**，each **86 过 / 9 败**。与 round5、旧冻结 f66c5d7 的 regression-summary.tsv、r3 冻结的 regression-seq-nobuild-001.tsv 和 nobuild-each-003.tsv 逐项比较，rc 没有一项不同，测试名也相同
- 失败的 9 项：8 项 virtio/g7 加 g2-irq。seq 方式下 stdout 为 0 字节的仍是那 7 项
- 补做：在 428d604 的全新克隆里单独跑 g6-mmio-map、g6-virtio-modern-caps、g6-virtio-rx-abi、g9-runtime-service-abi，4 项都是 rc=1，报错 `ld returned 1`。这证实了报告说的「同类 3 项」确实存在，并且已经被 fe3e3cd 修好
- 注意：r3 冻结目录 logs/ 下没有叫 regression-summary.tsv 的文件，用的是 seq 和 each 两个 tsv
- 结论：**符合**

## E. 启动（logs/runs/、E-ovmf-fake.txt）
- a 两次、b 一次，都 exit 0、qemu_exit 124。三份串口 sha256 都是 9d7445be…2946，与冻结 boot-clean-a/run-001、run-002、boot-clean-b/run-001 一致
- 串口内容：FIRMWARE 行、两次 EXIT BOOT SERVICES、`KERNEL C OK`、`AFOS-M1 version=0.1.0-dev build=fe3e3cd` ✓
- 日志头字段（门槛 8）：accel=tcg、qemu_version、ovmf_code/vars 路径和 sha256、ovmf_package_claim、image sha256、kernel_build_info、qemu_cmd、qemu_exit、ovmf_runtime_report、kernel_version_serial，齐全 ✓
- 伪造 changelog 实验（放 9999.99-FAKE）：日志头显示 `ovmf_package_claim=edk2 9999.99-FAKE (unverified: … can be edited or faked. The firmware identity is ovmf_code sha256)`；ovmf_code 的 sha256 不变（624e06de…）。伪造仍然能改变这个字段，但字段名和说明已如实写明「未验证、可伪造」，README 和报告也这么写 ✓
- 报告说固件与 .deb 的 md5sums 一致，这一点我没核
- 结论：**符合**

## F. 报告措辞（logs/F-disasm.txt）
- (a) 第一条 cli：`__kernel_text_start` 0x100000 是 cli，接着 lea rsp、and、`call 10408e <kernel_main>`。报告一致 ✓
- (b) 结尾：kernel_main 在 0x1043e8 调 serial_print（打印 RING3 ABI READY）后，0x1043ed `hlt`、0x1043ee `jmp 1043ed`。报告「kernel_main 在 0x1043ed 执行 hlt 循环，循环体里只有 hlt」一致 ✓。其余 hlt 在 kernel_entry.halt、scheduler_halt、kernel_exception_handler；sti 0 条
- (c) ConOut：报告写「根据只写串口的 FIRMWARE 行只出现一次推断，没有直接查证」，如实 ✓
- (d) 门槛 4 报告没列（沿用上一轮的部分满足）；第 5 条部分满足；门槛 11 部分满足。我认为合适
- r2 九项对照表逐条：
  1. 已修复：调用方环境（含标记变量）挡住 ✓。但直接调内层时自带 SYSTEM_DIRS 可以绕过目录约束（见 B），报告没提
  2. 「调用方 PATH 不再继承」✓；「toolchain-dir 防不住、只做到可见」✓
  3. ✓（D 节）
  4. ✓（C 节）
  5. ✓（E 节；md5 一致性没核）
  6. ✓：换 shell 后 SHA256SUMS 相同，前提是 `--toolchain-dir` 写法相同
  7. 未改，按预期
  8. 已加目录外的 FREEZE-HASHES.txt，但这个文件本身可写、没有校验（见 G）
  9. --trace 能跟进内层（400 行 `+`），普通 `bash -x` 只跟到外层 23 行；提示里有 sha256sum ✓；措辞已改 ✓
- 结论：**基本符合**（第 1 条说法略宽）

## G. 冻结（logs/G-freeze.txt、check-*.txt）
- 旧冻结 f66c5d7：`sha256sum -c` exit 0，134 条，自身 dfa2efb8… ✓
- 旧冻结 428d604：exit 0，255 条，自身 65dcb207…ef13 ✓
- 新冻结 fe3e3cd：exit 0，0 项失败，**626 条**；没有清单外的文件，也没有清单有但缺失的文件；没有符号链接、没有可写文件；目录 dr-xr-xr-x，属主 构建用户
- 自身哈希 ae09ceb2…36cd8，等于 FREEZE-HASHES.txt 里标「当前有效」的那条 ✓。BASELINE 里的报告副本与 T-M1-0-REPORT.md 一致；patches/ 与 patches-r3/ 一致
- 作废的 10c4c96c…：说明是「首次封存缺 boot-clean-b 串口」。旧内容已被覆盖，我无法核对。把当前 SHA256SUMS 去掉 boot-clean-b 的 3 行再算哈希，得到 4ca6445a…，≠ 10c4c96c…，可能还有别的差异（比如 header 文件不同）。**无法判断**这个哈希对应什么
- FREEZE-HASHES.txt 本身：-rw-r--r--，17:30 修改，可写，没有被任何清单覆盖或签名；冻结目录内的 BASELINE.txt 和报告副本里提到了它
- 结论：**符合**（冻结与校验都通过）；目录外哈希的保护力度有限，已如实披露

## H. 改动面（logs/H.txt）
- `git diff --stat 428d604 fe3e3cd`：
  - 4 个测试各 +1 行，只有 mkdir -p
  - README +4/-2
  - 新增 build-m1-default-inner.sh（125 行）
  - build-m1-default.sh +46/-76
  - run-m1-uefi.sh +3/-3（字段改名、加说明）
- 没有无关改动；4 个测试的 diff 只有 mkdir 行，没有改断言 ✓
- product/kernel 没有改动 ✓
- kernel.elf 与 428d604 冻结版比：只差 7 字节，位于偏移 60169–60175（0xEB09 起），就是版本串里的构建号 `428d604`→`fe3e3cd`；.text、.data 一致，.rodata 不同；`objdump -d` 全文 diff 0 行 ✓。BOOTX64.EFI、initrd.bin 相同；img 差 7 字节
- 作者：afos-codex
- 名字检查：用 rg 搜项目负责人的中文全名、名字和拼音，范围是 diff、新冻结目录、报告、FREEZE-HASHES、fe3e3cd 全树，0 命中；/home 非 构建用户 路径 0；product 下 `<工作区>/` 0
- 脚本里的硬编码：build-m1-default.sh:23 `system_dirs="/usr/bin:/bin"`；外层固定调 /usr/bin/env、/bin/bash；HOME=/nonexistent。这些是通用系统路径，不是盒子专属路径
- 结论：**符合**

## I. 退出码（logs/I-exitcodes.txt）
- 不给 `--toolchain-dir`：3（missing nasm clang lld-link file，提示里有 sha256sum 和搜索路径）✓
- 空目录作 toolchain-dir：3 ✓
- 相对路径：2
- 白名单不符（直接调内层，带脏环境或 PATH 不符）：4 ✓
- 5：正常调用几乎触发不到，因为 command -v 的结果必然落在 PATH 里的目录。实测 `--toolchain-dir=<评估目录>/env/bin/`（**末尾带斜杠**）时返回 5，报 `nasm=<评估目录>/env/bin/nasm` 不在预期目录。这是比较字符串造成的误报：合法目录多写一个斜杠就被拒
- `bash -x build-m1-default.sh`：只跟踪外层（23 行），跟不进内层；`--trace` 能跟进内层（400 行）✓
- 附带发现：`--out=DIR` 会先 `rm -rf DIR`，不做任何检查。实测 `--out=<临时目录>/r3/victim`，里面原有的 keep.txt 被删了。按代码推断，`--out=.` 会删掉当前目录（比如仓库根）。我**没有实测**这种危险写法
- 结论：**符合**（3、4、5 都实测到；5 有斜杠误报）

## 仍存在的缺陷与疑点
1. **重要**：`--toolchain-dir` 不限制目录来源。符号链接、`..` 伪装、多给一个前置目录，都能用假工具改变产物，版本行不变，只有 tool_* 的 sha256 和路径可见。直接调内层时，SYSTEM_DIRS 参数也能塞进假目录。报告承认「只做到可见」，但没写「预期目录完全由调用方决定」。证据：B-inject2.txt（two_TC_fake_first、inner_direct_sysdirs_spoof）、B-inject3.txt
2. **重要**：`--out=DIR` 先 rm -rf 而不做检查，误传会删数据（实测删掉了 victim/keep.txt；`--out=.` 按代码推断会删仓库根）。证据：I-exitcodes.txt；build-m1-default-inner.sh 里的 `rm -rf "$out_dir"`
3. **次要/重要**：仓库 .git/config 和 index 能影响构建。core.fsmonitor 指定的任意命令在构建时被执行；assume-unchanged 能隐藏源码修改，版本行没有 -dirty（img 40af84bf）。报告列为「未穷举」。证据：B-gitconfig.txt
4. **次要**：工具版本是 `--version` 自报的，可以伪造（clang 99.0.0 fake）；sha256 不覆盖被包装的真实二进制和共享库。证据：B-inject3.txt
5. **次要**：`--toolchain-dir` 末尾带斜杠会误报 exit 5
6. **次要**：build-env.txt 和 build-info.txt 跟着 `--toolchain-dir` 的字符串走，工具链装在别的位置时，冻结 SHA256SUMS 里这两项对不上（镜像不受影响）
7. **次要**：FREEZE-HASHES.txt 自身可写、没有校验；作废哈希 10c4c96c… 无法复核
8. **次要**：测试用的 build-bios.sh 仍然吃 NASMENV/CPATH（对照实验证明），所以测试构建没有净化。这不影响默认镜像，但测试结果对调用方环境敏感
9. 门槛 6：MIT 和 LICENSE 本轮不审，记「无法判断」

## 没来得及核的
- 固件与 .deb md5sums 一致性（logs/ovmf-identity-001.log）没有复核
- 替换工具链共享库（libclang-cpp、libLLVM）能不能被 sha256 发现，没有测
- .git/config 其他项（hooks 不在构建路径；core.attributesFile、filter 等）没有穷举
- `--out=.`、`--out=/` 这类危险写法按规则没有实测，只是读代码推断
- KVM、其他版本的 OVMF 没测

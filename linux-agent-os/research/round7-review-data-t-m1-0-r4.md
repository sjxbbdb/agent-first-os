> 入库说明：验收审查 t-m1-0-r4 的数据记录原文，只做路径、构建用户名、账号的替换（见 `research/round7-stage-reviews.md` 第二节）。日志文件没有入库，哈希见 `research/round7-review-logs-sha256.txt`。

# T-M1-0 r4 差异核对（t-m1-0-r4 = cc80bb3，基于 fe3e3cd）
时间：2026-11-02 18:22 起（UTC+8）。工作目录 <临时目录>/r4/，原始输出 logs/。工具链 `--toolchain-dir=<评估目录>/env/bin`。

## A1 干净克隆复现 —— 符合
命令：`git clone -b t-m1-0-r4 work {a,b}`；各自 `bash product/tools/build-m1-default.sh --toolchain-dir=$TC`；a 原地重建；b 用 `env -i PATH=/usr/bin:/bin unshare -rn` 断网构建。（logs/A1.txt）
- a、b：HEAD=cc80bb3，porcelain 0 行，无 build/；4 次构建均 exit 0。
- 镜像 `bb8fb93610bbee9a96f33b63cf539d52f8298d478898bd6a6fa7dfedc1282eb6`，a/b/重建/断网/冻结 artifacts/ 全部一致；BOOTX64.EFI、kernel.elf、initrd.bin、build-info.txt、build-env.txt 也都与冻结一致（不同克隆路径下 build-env 也一致，r3 的路径相关问题没有再出现）。
- 与 fe3e3cd 冻结 kernel.elf 比：`cmp -l` 有 7 字节不同，偏移 60169–60175（连续 7 字节，即 build= 后的 7 位短哈希）；objdump -d 去掉文件头后 diff 为 0 行；objcopy 提取的 .text、.data 都相同；版本串 `AFOS-M1 version=0.1.0-dev build=cc80bb3`。EFI、initrd 与 fe3e3cd 相同。

## A2 --out 保护 —— 基本符合（有一个次要缺口）
读码（build-m1-default-inner.sh 第 73–98 行）：`out_real=realpath -m`（会解析符号链接），拒绝条件：repo_real 等于 out_real 或在它之下；out_real 为 `/`；已存在但不是目录；非空且没有 `.afos-m1-build-out`。上述检查都在 `rm -rf` 之前，退出码 6。`--out=/` 先读码确认会命中 `[[ $out_real == / ]]`，然后才实测（m1-build-out-guard-test.sh 自己也测了这一项）。
在 <临时目录>/r4/g/repo（一次性克隆）中实测（logs/A2-cases.txt）：
| 用例 | rc | 结果 |
|---|---|---|
| 仓库根绝对路径 / `.` / `..`（从 product/ 运行）/ 父目录 / `/` / 指向仓库根的符号链接 / 符号链接加末尾 `/` / `仓库根/product/..` / `$PWD/./build/..` / `--out=`（空串，等于 PWD） | 6 | 拒绝 |
| 普通文件 | 6 | 拒绝 |
| 非空且无标记（放了哨兵） | 6 | 哨兵仍是 keep |
| 带标记的目录（另放 stale.txt） | 0 | 允许；stale.txt 已被清掉，镜像生成 |
| 空目录 / 空目录加末尾 `/` / 相对路径 build/rel-out | 0 | 允许 |
| 仓库内非空子目录 product、.git | 6 | 拒绝 |
| 大写 /TMP/... | 1 | 路径不存在，mkdir 失败（大小写敏感的文件系统，不构成绕过） |
| **仓库内的空子目录 `.git/refs/tags`** | **0** | **允许**；构建产物被写进 refs/tags，`git fsck` 报 `refs/tags/BOOTX64.EFI: invalid sha1 pointer`，`git for-each-ref` 报 broken ref |
每个用例跑完都核了：被跟踪文件 `git status` 都是 0 行，仓库根哨兵没变；被拒的用例前后，仓库树（不含 .git）文件哈希都是 45d2a2bc…（879 个文件）；之后的增加只是 build/ 下的中间产物。
bind mount、硬链接：要 root 或者对目录做硬链接，不可行，没有测。
m1-build-out-guard-test.sh：exit 0，全部 ok（logs/A2-guard-test.log）。有对照组（空目录、新目录、带标记目录重建、默认目录两次构建要成功，还要比对镜像），断言 `rc==6` 加拒绝消息。没有覆盖的项：符号链接、`--out=`空串、带标记目录里的旧文件被清理（它只断言重建成功）、仓库内的空目录；另外这个用例没有加入 run-all.sh（`grep -c out-guard run-all.sh` = 0）。

## A3 -dirty 判定 —— 对被跟踪文件符合；未跟踪文件可以改变产物而不出 -dirty（重要）
logs/A3.txt、A3-redo.txt。（A3.txt 的 c1–d1 被污染：我的 reset 先执行了 checkout、后清的 assume-unchanged 位，kernel.c 没有恢复。已在新克隆 d2 里重做这几项，以 A3-redo 为准。）
| 情形 | 版本行 | 镜像 |
|---|---|---|
| f 干净 | build=cc80bb3 | bb8fb936 |
| a assume-unchanged 后改 kernel.c（git status 0 行） | cc80bb3-dirty（content kernel.c） | 变了 |
| b skip-worktree 同上 | cc80bb3-dirty | 变了 |
| c1 新增未跟踪 .c | cc80bb3 | bb8fb936（不参与编译） |
| **c2 新增未跟踪 product/kernel/include/stddef.h（`#define AGENT_OS_TEST_RING3 1` + `#include_next`）** | **cc80bb3（不出 dirty）** | **2966103c，kernel.elf 出现 'RING3 launch' 串** |
| c3 被 .gitignore 忽略的 build/ignored.txt | cc80bb3 | bb8fb936 |
| d1 改 initrd 来源 echo.manifest.json | cc80bb3-dirty | 变了 |
| d2 改 product/tools/build-uefi.sh | cc80bb3-dirty | 变了（仅版本串） |
| e 只做 chmod -x | cc80bb3-dirty（mode） | — |
| 恢复后 | cc80bb3 | bb8fb936 |
执行命令的 git 配置：core.fsmonitor=写计数的脚本、core.hooksPath（放了 4 个钩子）、diff.external、core.pager、core.sshCommand、include.path（里面有 alias ls-tree 和 core.attributesFile→`* filter=evil`）、filter.evil.clean/smudge/process。构建后 fsmon.count=0、cmd.count=0，产物 bb8fb936，未变。对照组：同一配置下手动 `git status`，fsmon=2、cmd=1，说明这些配置确实是生效的。
构建脚本里的 git 调用（rg）：build-m1-default-inner.sh:103 `git -c core.fsmonitor=false rev-parse HEAD`；build-bios.sh:55 `rev-parse --short=7 HEAD`；worktree-dirty.py:20 `ls-tree -r -z --full-tree HEAD`。都不会触发 diff、pager、filter、hooks。
另外，HOME=/nonexistent 加 GIT_CONFIG_NOSYSTEM=1，用户级和系统级 gitconfig 都不会被读，只剩仓库的 .git/config。
README 写的是「未跟踪文件不算脏」，但没有写明未跟踪的头文件会被 -I 优先命中、进而改变产物。

## A4 --toolchain-dir —— 符合（README 如实）
logs/A4.txt：末尾 `/`、`//`、`.`、`..` 都规范成 <评估目录>/env/bin，rc=0，镜像和 build-env 都与冻结一致（末尾斜杠误报退出 5 的问题已修）。符号链接不解析，记录的就是链接路径（build-env 跟着变，镜像不变）；`$TC/../../../..<临时目录>/r4/tclink` 规范后记录为 <临时目录>/r4/tclink。相对路径 rc=2。两个目录、假 nasm 放在前面（wrapper 加 -DAGENT_OS_TEST_FAULT_RECOVERY）：rc=0，镜像 64f1ed87（产物被改了），只在 toolchain_dirs 里看得出。
直接调用内层：SYSTEM_DIRS 带假目录 → rc=2「SYSTEM_DIRS must be /usr/bin:/bin」；TOOLCHAIN_DIRS 带末尾 `/` → rc=2；TOOLCHAIN_DIRS 指向假目录 → rc=0，产物 64f1ed87。
README 原文（逐句已摘入 logs/A4.txt）：「预期工具目录由调用方用 --toolchain-dir 决定…都能改变产物，版本行不变；只有 build-info…能看出来」「tool_*_version…可以伪造」「直接调用内层…SYSTEM_DIRS 必须是 /usr/bin:/bin…TOOLCHAIN_DIRS 本身仍由调用方决定」「规范化…不解析符号链接…换一个工具链路径，这两个文件…就会不同（镜像不受影响）」「测试用的构建…没有做环境净化」。这些说法与实测逐条一致。挡住的：SYSTEM_DIRS 被扩大、路径写法不规范；挡不住的：调用方给的工具目录里放了什么。README 都写了。

## A5 —— 符合
m1-build-out-guard-test.sh 通过（见 A2）。95 项回归在 B8 一起跑。

## A6 冻结 —— 符合
logs/A6.txt：`sha256sum -c` 结果：f66c5d7 134/0 失败，428d604 255/0，fe3e3cd 626/0，cc80bb3 264/0，全部 exit 0。cc80bb3 目录共 265 个文件（264 加 SHA256SUMS），没有清单外文件，也没有缺的；可写位 0 个，符号链接 0 个，目录权限 dr-xr-xr-x。各目录 SHA256SUMS 自身的哈希：dfa2efb8 / 65dcb207 / ae09ceb2 / e45b213c，与 FREEZE-HASHES.txt 一致。FREEZE-HASHES.txt 是 0444 的文本，没有任何外部校验（同 r3）。
作废哈希复算：`python3 recompute-void-hash.py freeze/…fe3e3cd` → entries 622，old_report_sha256 fd028170…，first_seal = **10c4c96cc1aeda2e8d63c36ce2eb6342188cfccd159fb7920882887cba4fef17**，与 FREEZE-HASHES 作废行一致。脚本里 `10c4c96c` 只出现在第 3 行注释中，计算本身没有用到目标值：它从现有 SHA256SUMS 删掉 4 行（logs/boot-clean-b/* 3 个和 run-004…log），再把报告那一行换成删掉「一处失误…第一个已作废。」一段后的报告哈希。由于 sha256 无法倒推，独立算出的值与先前记录的值一致，可以证明这个重建与首封时的内容逐字节相同。（局限：它只证明首封 = 现封减 4 行，再换报告；不证明首封时这些文件的内容是正确的。）

## A7 改动面 —— 符合
`git diff --numstat fe3e3cd cc80bb3`：m1-build-out-guard-test.sh +53（新增）、README +9/-1、build-bios.sh +5/-2（判脏）、inner +41/-1（SYSTEM_DIRS、TOOLCHAIN 校验、--out 保护、git -c）、outer +3（realpath -s）、worktree-dirty.py +58（新增）。product/kernel 无改动，.text 一致（A1）。没有看到无关改动；现有测试一行未删改（tests 下只新增一个文件）。作者和提交者都是 afos-codex <afos-codex@localhost>。姓名、拼音的 grep（diff、冻结目录、报告、FREEZE-HASHES、cc80bb3 树）0 命中；/home 非 构建用户 0 命中；product 代码中 <工作区>/ 0 命中（README 以外）。

## A8 门槛 6 —— 无法判断（LICENSE 不在被审分支上）
- work 仓库的 t-m1-0-r4、t-m1-1-r4、main 里都没有 LICENSE（`fatal: path 'LICENSE' does not exist`）。
- 599e2da 只在 <规格仓库> 的**本地 main** 上（`git branch -a --contains` 只有 `* main`）；origin/main 是 494db93（fetch 之后），599e2da 还没推送。作者 agent-os-spec <noreply@users.noreply.github.com>。它同时把规格、任务清单升到 v0.9（origin/main 为 v0.8）。
- LICENSE 内容：去掉版权行、规范化空白后，与 SPDX MIT 标准文本逐字一致。版权行原文：`Copyright (c) 2026 agent-first-os contributors`，**不含人名**。
- 新增文件 SPDX 行均为 `# SPDX-License-Identifier: MIT`（build-m1-default.sh、run-m1-uefi.sh、make-esp-image.py，以及本轮新增的 inner、worktree-dirty.py、guard-test）。
- LICENSE_MATRIX.md：599e2da 中已更新为「MIT…署名 agent-first-os contributors」，但这也只在本地 main 上，work 分支没有。决定 0005 记的是「来源就是总经理的转达，无独立原始记录」（只记录事实）。

## 仍存在的缺陷与疑点
1. 【重要】未跟踪文件可以改变产物而不出 -dirty：在 product/kernel/include/ 放一个未跟踪的 stddef.h，可以打开测试宏（镜像 2966103c，版本行仍是 cc80bb3）。证据：logs/A3-redo.txt c2。README 只写了「未跟踪文件不算脏」。可行的修法：-dirty 也检查源码目录下的未跟踪文件，或者在克隆好的干净树里构建。
2. 【次要】--out 允许仓库内、包括 .git 里的空目录（`.git/refs/tags`），删掉后会写入产物，破坏 refs（fsck 报错）。证据：logs/A2-cases.txt dotgit_empty_subdir。
3. 【次要】guard-test 没有覆盖符号链接、空串、旧文件清理和仓库内空目录，也没有接入 run-all.sh。
4. 【次要／流程】LICENSE 只在规格仓库本地未推送的提交 599e2da 上，不在送审的 work 分支；门槛 6 无法据此判定。
5. 【次要，沿用】FREEZE-HASHES.txt 本身没有外部校验；工具目录内容由调用方决定（README 已如实写明）。

## 没来得及核的
bind mount、硬链接绕法（需要 root，没做）；--out 的 TOCTOU 竞态；worktree-dirty.py 对子模块、gitattributes eol 的处理。

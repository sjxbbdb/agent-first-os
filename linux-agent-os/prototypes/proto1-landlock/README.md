# Landlock 原型 1 —— 可入库导出包

内容：`src/` 测试程序、`initramfs-src/` /init 脚本、`scripts/` 构建与运行脚本、`env.sh`、`docs/`（README/RESULT/GPL_AND_FILES）、`logs/`、`artifacts/`。
送审基准是冻结目录 frozen-v1（不在本包内，原始未脱敏材料与二进制都在那里）。本包里的完整工作流说明见 docs/README.md。

## 不入库的东西
bzImage、initramfs cpio、landlock_test 二进制、内核/busybox tarball、tools/（micromamba 环境、qemu-root）、build/。哈希与获取方式见 artifacts/SHA256SUMS-binaries.txt。

## logs/
- logs/：送审用的 c3ctl 两份串口日志（qemu-t2-serial-c3ctl-run1/2.log）、frozen-v1 复核日志、内核/busybox/测试程序/initramfs 构建日志、下载校验日志、工具安装日志。
- logs/history/：**历史记录**，加 C3c 之前的 T1/T2 串口日志和构建日志，对应的旧产物已不存在，不作送审依据。

## 脱敏说明（重要）
export/logs、export/docs、export/artifacts 是**脱敏后的副本**，文件哈希与原件不同。原始日志在 frozen-v1/logs/（和项目 logs/）。对应关系：同名文件一一对应（history/ 下对应项目 logs/ 同名文件，frozen-v1 只收了 c3ctl 版及构建日志）。
脱敏规则（按顺序作用于 logs/ docs/ artifacts/ 下全部文件；规则里涉及的原始用户名、路径、主机名为避免入库，这里只写类别，不写原文；做这些替换的 sed 脚本是内部工具，不随本包入库）：
1. 项目工作目录的绝对路径替换为 `$PROJECT`。
2. 构建用户的家目录路径替换为 `$HOME_DIR`。
3. 构建机的「用户@主机名」替换为 `builder@host`，主机名单独出现时替换为 `host`。
4. `ls -l` 输出里的属主和属组、`id` 输出里的用户名、文档里提到构建用户的字样，替换为 `builder`。
5. 其余以工作区根目录开头的绝对路径替换为 `$WORKSPACE`。
`$PROJECT`、`$HOME_DIR`、`$WORKSPACE`、`builder`、`host` 都是字面占位符，不是可展开变量。
脱敏没有改动测试结果行（[C0]..[C4]、[C3c]、汇总）、LANDLOCK_ABI 行、`landlock: Up and running.` 行、内核版本号 6.18.60、各校验和与 `OK` 行、BUILD_RC、qemu exit code：脱敏前后抽取这些关键行比较完全一致（共 180 行）。唯一触及的内核行是 `Linux version 6.18.60 (…)` 里的构建者 user@host 被换成 `builder@host`，版本号本身没变。
入库前用个人信息扫描命令全量检查：原始用户名、路径、主机名字样已从规则说明和脚本里去掉，不再有意保留。

## logs/ 里的第三方署名
logs/ 里保留了内核启动输出和 gpg 校验输出自带的公开署名与邮件地址（内核维护者、kernel.org 签名密钥、驱动作者），它们是原文输出，属于验收证据，不是项目人员，未做修改。

## 入库说明
本目录是导出包入库后的版本，与原导出包相比有三处改动：删除 scripts 下的脱敏 sed 脚本，重写上面的脱敏规则说明，重新生成 MANIFEST.txt（40 条，不含自身）。其余文件与原导出包逐字节相同。

## 脚本
scripts/ 原本就用 `$(dirname "$0")/..` 定位项目根；唯一的绝对路径在 env.sh（`LP=`），导出版改为 `LP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"`。所有 .sh 已过 `bash -n`；导出版没有实跑（实际跑通的是 frozen-v1 / 项目目录里的原版，区别只有 env.sh 这一行）。

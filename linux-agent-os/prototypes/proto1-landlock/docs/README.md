# Landlock 原型 1（T1 + T2）

在 QEMU 虚拟机里验证：非特权式自我限制的 Landlock 文件系统规则生效、越权被拒（EACCES）、限制被 execve 的子进程继承。零内核补丁。
规格：`specs/proto1-landlock-task-sandbox.md`，任务：`tasks/proto1-landlock-tasks.md`（T1、T2）。结论与证据见 `RESULT.md`。

## 目录
```
env.sh                 复跑前 source（PATH、qemu 库路径、版本号）
scripts/00-install-tools.sh      micromamba + conda-forge 装 make/flex/bison/bc/cpio/m4/elfutils/openssl/pkg-config/bzip2/zlib
scripts/00b-install-qemu-debs.sh 从 deb.debian.org(trixie) 取 qemu-system-x86 及缺失依赖 .deb，dpkg -x 解包到 tools/qemu-root
scripts/01-download.sh           下载内核/busybox，sha256 核对，PGP 校验
scripts/02-build-kernel.sh       defconfig + scripts/config 调整 + make -j8 bzImage（+ make headers）
scripts/03-build-busybox.sh      busybox 源码静态编译
scripts/04-build-test.sh         静态编译 src/landlock_test.c
scripts/05-build-initramfs.sh    t1|t2 两种 initramfs（gen_init_cpio + gzip）
scripts/run-qemu.sh              t1|t2 <日志名>，串口全量落 logs/
scripts/run-all.sh               全流程一次跑完
src/landlock_test.c              测试程序（GPL-2.0-only）
initramfs-src/init-t1, init-t2   两个阶段的 /init
logs/ artifacts/                 日志与产物记录
build/ dl/ tools/                构建目录、下载、工具（体积大，未必需要送审）
```

## 环境准备（无 root、无 apt）
盒子：Debian 13.7，gcc 14.2.0（自带），glibc 静态库 `/usr/lib/x86_64-linux-gnu/libc.a` 已存在（所以 busybox 和测试程序直接 `-static`，没用 musl）。
- 构建工具：`scripts/00-install-tools.sh`。下载 micromamba 2.10.0 静态二进制（盒子没 bzip2，用 python tarfile 解包），
  `micromamba create -p tools/env -c conda-forge make flex bison bc cpio m4 bzip2 elfutils openssl pkg-config zlib`。
  实装版本：make 4.4.1、flex 2.6.4、bison 3.8.2、bc 1.07.1、cpio 2.15、m4 1.4.21、elfutils 0.197、openssl 3.6.5、pkg-config 0.29.2、bzip2 1.0.8、zlib 1.3.2（日志 logs/tools-install.log）。
- QEMU：conda-forge/bioconda 均没有 qemu 包；`apt-get download` 报 `Unable to locate package`（apt 列表坏了）。
  改为 `scripts/00b-install-qemu-debs.sh`：下载 trixie 的 Packages.xz，python 解析依赖，只拉盒子上没装的包（共 24 个 .deb），`dpkg -x` 解到 `tools/qemu-root`（不是安装，不动系统）。版本 QEMU 10.0.13 (Debian 1:10.0.13+ds-0+deb13u1)。运行时用 `LD_LIBRARY_PATH=tools/qemu-root/usr/lib/x86_64-linux-gnu`（只对 qemu 生效）以及 `-L` 指向解包出的 BIOS/ipxe 目录。日志 logs/tools-install-qemu.log。
- KVM：`/dev/kvm` 存在，但属 `root:pulse-access 0660`，builder 用户不在该组 → 不可用，全部用 `-accel tcg`（run-qemu.sh 自动判断并写进日志头）。

## 从零复跑
```bash
mkdir -p $PROJECT && cd $PROJECT   # 放入本仓库的 scripts/ src/ initramfs-src/ env.sh
bash scripts/run-all.sh
# 或分步：
bash scripts/00-install-tools.sh   > logs/tools-install.log 2>&1
bash scripts/00b-install-qemu-debs.sh > logs/tools-install-qemu.log 2>&1
bash scripts/01-download.sh        > logs/download-verify.log 2>&1
nohup bash scripts/02-build-kernel.sh > logs/kernel-build.log 2>&1 &   # 约 3 分钟（8 核）
bash scripts/03-build-busybox.sh   > logs/busybox-build.log 2>&1
bash scripts/04-build-test.sh      > logs/test-build.log 2>&1         # 依赖内核目录里 make headers 的结果
bash scripts/05-build-initramfs.sh t1 > logs/initramfs-build-t1.log 2>&1
bash scripts/05-build-initramfs.sh t2 > logs/initramfs-build-t2.log 2>&1
bash scripts/run-qemu.sh t1 qemu-t1-serial.log
bash scripts/run-qemu.sh t2 qemu-t2-serial-run1.log
bash scripts/run-qemu.sh t2 qemu-t2-serial-run2.log
```
追加 C3c 对照后（不重编内核）：
```bash
bash scripts/04-build-test.sh > logs/test-build-c3ctl.log 2>&1
bash scripts/05-build-initramfs.sh t2 > logs/initramfs-build-t2-c3ctl.log 2>&1
bash scripts/run-qemu.sh t2 qemu-t2-serial-c3ctl-run1.log
bash scripts/run-qemu.sh t2 qemu-t2-serial-c3ctl-run2.log
```
说明：qemu-t2-serial-run1/2.log 是加 C3c 之前的程序版本跑出来的（旧 sha256 见 artifacts/*.pre-c3c.sha256），当前源码重跑得到的是 c3ctl 日志那样的输出。
QEMU 实际命令（见每份串口日志第 3 行）：
```
LD_LIBRARY_PATH=tools/qemu-root/usr/lib/x86_64-linux-gnu timeout 300 qemu-system-x86_64 -nographic -no-reboot -accel tcg -m 512 \
  -L tools/qemu-root/usr/share/qemu -L tools/qemu-root/usr/share/seabios -L tools/qemu-root/usr/lib/ipxe/qemu \
  -kernel build/linux-6.18.60/arch/x86/boot/bzImage -initrd build/out/initramfs-t2.cpio.gz \
  -append "console=ttyS0 lsm=landlock,lockdown,yama,loadpin,safesetid,selinux,smack,tomoyo,apparmor,ipe,bpf panic=-1" 2>&1 | tee logs/xxx.log
```
注意：本 README 的命令序列是按实际跑过的分步脚本整理的；`run-all.sh` 这个合并脚本本身没有从空目录完整跑过一遍。

## 耗时（本机实测）
| 步骤 | 耗时 |
|---|---|
| conda 工具安装 | 约 1 分钟 |
| qemu .deb 拉取解包 | 约 2 秒 |
| 内核 make -j8 bzImage | 184 秒（日志 BUILD_RC=0 ELAPSED_S=184） |
| busybox 静态编译 | 11 秒 |
| 测试程序 / initramfs | 各 1~2 秒 |
| QEMU TCG 单次启动到关机 | 约 3~5 秒 |

## 坑与处理
1. conda-forge 没有 qemu；apt 列表损坏 → 直接从 deb.debian.org pool 取 .deb + `dpkg -x`。
2. 盒子没 bzip2 → micromamba 包用 python tarfile 解；之后 conda 装 bzip2 供 busybox tarball。
3. 内核 `headers_install` 需要 rsync（没有）→ 改用 `make headers`，头文件在源码树 `usr/include`。
4. objtool 缺 `gelf.h` → conda 装 elfutils+zlib，make 时传 `HOSTCFLAGS=-I tools/env/include HOSTLDFLAGS="-L... -Wl,-rpath,..."`。
5. busybox 脚本 `yes "" | make oldconfig` 在 pipefail 下 SIGPIPE 退出码 141 → 该行局部关掉 pipefail。busybox 的 tc applet 预防性关闭（新内核头已去掉 CBQ，常见编译错误）。
6. 非 root 不能 mknod `/dev/console`，没有它内核执行 /init 前打不开控制台 → 用内核源码里的 `usr/gen_init_cpio.c`（单独编译，不改源码）按清单生成 cpio，加 `nod /dev/console`，所有文件归 0:0，`-t 0` 固定时间戳。
7. 解包的 qemu 里 BIOS 是指向 /usr/share/seabios 的绝对符号链接 → 额外 `-L` 指向解包目录。
8. kernel.org 的 WKD 取 key 网络不通 → 从 keyserver.ubuntu.com 按指纹取 Greg KH（647F28654894E3BD457199BE38DBBDC86092693E）与 kernel.org checksum autosigner 公钥；Linus 的 key 取失败（不影响，本 tarball 是 Greg 签的）。
9. defconfig 的 CONFIG_LSM 原本就含 landlock（`landlock,lockdown,yama,loadpin,safesetid,selinux,smack,tomoyo,apparmor,ipe,bpf`），只是 `CONFIG_SECURITY_LANDLOCK` 默认没开。命令行 lsm= 用 `landlock,<原列表去掉重复的 landlock>`。实际初始化的 LSM 是 `capability,landlock,selinux`（defconfig 只编进了 SELinux，其余列表项未编译，被忽略）。

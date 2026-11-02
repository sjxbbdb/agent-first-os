# GPL 说明与改动文件清单

## 内核
- Linux 6.18.60，GPL-2.0（含 syscall note）。**内核源码零改动、零补丁**：只解压官方 tarball，用 `make defconfig` + `scripts/config` 生成 .config。
- 唯一"用到"的内核源码文件：`usr/gen_init_cpio.c` 被单独编译成主机工具来打包 initramfs，未修改；测试程序 include 了 `make headers` 导出的 uapi 头 `linux/landlock.h`（uapi 头带 Linux-syscall-note，用户态程序包含它不构成衍生作品）。
- 源码获取：https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.60.tar.xz ，配置见 artifacts/kernel.config。

## 本项目新增文件（全部是新文件，无对他人文件的修改）
- src/landlock_test.c —— 许可证 **GPL-2.0-only**（SPDX 写在文件头）。选它是因为参考了内核 samples/landlock/sandboxer.c 的写法，且要与 GPL busybox 一起打进 initramfs，统一用 GPL-2.0-only 最省事。
- initramfs-src/init-t1、init-t2、scripts/*.sh、env.sh、README.md、RESULT.md、本文件：项目自有脚本/文档，同按 GPL-2.0-only 对待。

## busybox（静态链接）
- 版本 1.36.1，源码 https://busybox.net/downloads/busybox-1.36.1.tar.bz2 （sha256 b8cc24c9574d809e7279c3be349795c5d5ceb6fdf19ca709f80cde50e47de314），GPL-2.0。
- 本项目从源码编译（`make defconfig`，改 CONFIG_STATIC=y、关 CONFIG_TC），未改源码，编译配置见 build/busybox-1.36.1/.config（脚本 scripts/03-build-busybox.sh 可完整重现）。
- 静态链接了 glibc（LGPL-2.1+）。GPL 义务：分发含 busybox 的 initramfs 时，需同时提供（或书面承诺提供）对应 busybox 源码与上述配置/构建脚本；glibc 静态链接按 LGPL 需允许用户重新链接，提供本项目全部构建脚本和目标文件可重建即满足。仅内部验证不分发时无对外义务。
- 测试程序同样静态链接 glibc，同理。

## 变更记录
- 2026-11-02 追加 C3c 对照：修改 src/landlock_test.c（新增 C3c 用例 2 行代码+1 行注释，汇总文案微调）；initramfs-src/ 下 init 脚本**未改**，但 initramfs-t2.cpio.gz 因内含新的测试程序而重新打包（新 sha256 见 artifacts/initramfs-t2.sha256）。内核源码与 bzImage 均未变动。

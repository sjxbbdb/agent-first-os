#!/bin/bash
# 解压内核（不改源码）-> defconfig -> scripts/config 调整 -> bzImage
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); source "$P/env.sh"
K="$P/build/linux-$KV"; cd "$P/build"
[ -d "$K" ] || tar -xf "$P/dl/linux-$KV.tar.xz"
cd "$K"
make mrproper
make defconfig
cp .config "$P/artifacts/defconfig.config"
grep -E '^CONFIG_LSM=|^CONFIG_SECURITY=|LANDLOCK' .config | tee "$P/artifacts/defconfig-lsm.txt" || true
C=scripts/config
$C -e SECURITY -e SECURITY_LANDLOCK -e DEVTMPFS -e DEVTMPFS_MOUNT -e PROC_FS -e SYSFS -e TMPFS \
   -e BLK_DEV_INITRD -e RD_GZIP -e BINFMT_ELF -e SERIAL_8250 -e SERIAL_8250_CONSOLE -e PRINTK \
   -d MODULES
# 在 defconfig 原有 LSM 列表最前面加 landlock（原有列表见 artifacts/defconfig-lsm.txt）
OLD=$(sed -n 's/^CONFIG_LSM="\(.*\)"/\1/p' "$P/artifacts/defconfig.config")
case ",$OLD," in *,landlock,*) NEW="$OLD";; *) NEW="landlock,$OLD";; esac
$C --set-str LSM "$NEW"
make olddefconfig
grep -E '^CONFIG_LSM=|^CONFIG_SECURITY=|LANDLOCK|^CONFIG_MODULES|DEVTMPFS|RD_GZIP|SERIAL_8250_CONSOLE' .config
cp .config "$P/artifacts/kernel.config"
scripts/diffconfig "$P/artifacts/defconfig.config" .config > "$P/artifacts/defconfig.diff" || true
make headers   # 导出 uapi 头到 usr/include（headers_install 需要 rsync，盒子没有）
T0=$(date +%s)
set +e
make -j8 HOSTCFLAGS="-I$TENV/include" HOSTLDFLAGS="-L$TENV/lib -Wl,-rpath,$TENV/lib" bzImage; RC=$?
set -e
echo "BUILD_RC=$RC ELAPSED_S=$(( $(date +%s)-T0 ))"; [ $RC = 0 ]
ls -l arch/x86/boot/bzImage; sha256sum arch/x86/boot/bzImage | tee "$P/artifacts/bzImage.sha256"
make -s kernelrelease | tee "$P/artifacts/kernelrelease.txt"

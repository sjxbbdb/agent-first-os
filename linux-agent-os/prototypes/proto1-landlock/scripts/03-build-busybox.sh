#!/bin/bash
# 源码静态编译 busybox（glibc 静态库 libc.a，盒子自带）
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); source "$P/env.sh"
cd "$P/build"; rm -rf busybox-$BB; tar -xjf "$P/dl/busybox-$BB.tar.bz2"; cd busybox-$BB
make defconfig
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
# tc 在新内核头下编译失败（CBQ 已从内核移除），关掉
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
(set +o pipefail; yes "" | make oldconfig >/dev/null)
grep -E '^CONFIG_STATIC|CONFIG_TC\b' .config || true
T0=$(date +%s); make -j8; echo "ELAPSED_S=$(( $(date +%s)-T0 ))"
readelf -lW busybox | grep -E "Elf file type|INTERP" || echo "无 INTERP 段 => 静态链接"; ls -l busybox; sha256sum busybox | tee "$P/artifacts/busybox.sha256"

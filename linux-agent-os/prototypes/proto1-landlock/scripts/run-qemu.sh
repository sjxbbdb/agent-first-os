#!/bin/bash
# 用法: run-qemu.sh t1|t2 <日志名>   串口全量输出经 stdio 落到 logs/<日志名>
set -uo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); source "$P/env.sh"; T=${1:-t2}; LOG="$P/logs/${2:-qemu-$T.log}"
# defconfig 原有 CONFIG_LSM 列表（已含 landlock，见 artifacts/defconfig-lsm.txt），按规格 lsm=landlock,<原有列表>，去重 landlock
LSM_ORIG=$(sed -n 's/^CONFIG_LSM="\(.*\)"/\1/p' "$P/artifacts/defconfig.config")
LSM="landlock,$(echo "$LSM_ORIG" | tr ',' '\n' | grep -vx landlock | paste -sd,)"
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then ACCEL="-enable-kvm"; else ACCEL="-accel tcg"; fi
CMD=(timeout 300 qemu-system-x86_64 -nographic -no-reboot $ACCEL -m 512 -L "$QEMU_DATA" -L "$LP/tools/qemu-root/usr/share/seabios" -L "$LP/tools/qemu-root/usr/lib/ipxe/qemu"
  -kernel "$P/build/linux-$KV/arch/x86/boot/bzImage" -initrd "$P/build/out/initramfs-$T.cpio.gz"
  -append "console=ttyS0 lsm=$LSM panic=-1")
{ echo "### date: $(date '+%F %T %z')"; echo "### /dev/kvm: $(ls -l /dev/kvm 2>&1); 选用加速: $ACCEL"
  echo "### cmd: LD_LIBRARY_PATH=$QEMU_LD ${CMD[*]@Q}"; } > "$LOG"
LD_LIBRARY_PATH="$QEMU_LD" "${CMD[@]}" < /dev/null 2>&1 | tee -a "$LOG"
RC=${PIPESTATUS[0]}; echo "### qemu exit code: $RC" | tee -a "$LOG"; exit $RC

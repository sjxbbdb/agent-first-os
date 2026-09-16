#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g7-native-block-write.log"
data_disk="$build_dir/g7-native-block-write-data.img"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G7_NATIVE_BLOCK_WRITE" \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G7_NATIVE_BLOCK_WRITE \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
# Seed the independent disk with a valid raw image so the same legacy
# virtio-blk path that proves sector-0 reads also has a known-good medium.
cp "$build_dir/stage1.img" "$data_disk"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -drive "if=none,id=vd0,format=raw,file=$data_disk" \
    -device virtio-blk-pci,drive=vd0 \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'G6 virtio block transport READY' "$log"
grep -Fq 'G7 NATIVE BLOCK WRITE START' "$log"
grep -Fq 'SYSCALL block write EINVAL' "$log"
grep -Fq 'SYSCALL block write ECAP' "$log"
grep -Fq 'SYSCALL block write EFAULT' "$log"
grep -Fq 'SYSCALL block write OK used completion' "$log"
grep -Fq 'G7 NATIVE BLOCK WRITE OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"

python3 -c 'import sys; data=open(sys.argv[1], "rb").read()[512:1024]; raise SystemExit("sector 1 does not contain the native fixture payload" if data != bytes([0xa5]) * 512 else 0)' "$data_disk"
echo 'PASS: Ring 3 block-write syscall validates capability/ABI/user buffer and reaches a real virtio used completion'

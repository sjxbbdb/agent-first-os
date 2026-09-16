#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g7-native-block-read.log"
data_disk="$build_dir/g7-native-block-read-data.img"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G7_NATIVE_BLOCK_READ" \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G7_NATIVE_BLOCK_READ \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
cp "$build_dir/stage1.img" "$data_disk"
# Keep sector 0 boot-valid for the existing probe check, then seed sector 1
# with a deterministic payload that the Ring 3 read fixture must observe.
dd if=/dev/zero of="$data_disk" bs=512 count=1 seek=1 conv=notrunc status=none
printf 'READOK01' | dd of="$data_disk" bs=1 seek=512 conv=notrunc status=none

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
grep -Fq 'G6 virtio block READ OK' "$log"
grep -Fq 'G7 NATIVE BLOCK READ START' "$log"
grep -Fq 'SYSCALL block read EINVAL' "$log"
grep -Fq 'SYSCALL block read ECAP' "$log"
grep -Fq 'SYSCALL block read EFAULT' "$log"
grep -Fq 'SYSCALL block read OK used completion' "$log"
grep -Fq 'G7 NATIVE BLOCK READ OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: Ring 3 block-read syscall validates capability/ABI/user destination and reads a real virtio sector'

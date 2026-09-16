#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g7-native-block-flush.log"
data_disk="$build_dir/g7-native-block-flush-data.img"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G7_NATIVE_BLOCK_FLUSH" \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G7_NATIVE_BLOCK_FLUSH \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
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
grep -Fq 'G7 virtio block FLUSH SUPPORTED' "$log"
grep -Fq 'G7 NATIVE BLOCK FLUSH START' "$log"
grep -Fq 'SYSCALL block flush EOPNOTSUPP' "$log"
grep -Fq 'SYSCALL block flush OK used completion' "$log"
grep -Fq 'G7 NATIVE BLOCK FLUSH OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: Ring 3 block-flush syscall checks opaque write capability/ABI and reaches a real virtio used completion'

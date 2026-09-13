#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-virtio-block.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
cp "$build_dir/stage1.img" "$build_dir/virtio-disk.img"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -drive "if=none,id=vd0,format=raw,file=$build_dir/virtio-disk.img" \
    -device virtio-blk-pci,drive=vd0 \
    -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
    -device virtio-keyboard-pci \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'G6 virtio block transport READY' "$log"
grep -Fq 'G6 virtio block READ OK' "$log"
grep -Fq 'G6 virtio net transport READY' "$log"
grep -Fq 'G6 virtio input transport DISCOVERED' "$log"
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: QEMU virtio-blk PCI legacy transport initializes queue 0, reads sector 0, and preserves Ring 3 boot'

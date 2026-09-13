#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-virtio-input.log"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G6_VIRTIO_INPUT" \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -device virtio-keyboard-pci \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
if grep -Fq 'G6 virtio input modern CAPABILITIES DISCOVERED' "$log"; then
    echo 'BLOCKED: modern virtio-input common/notify/device capabilities are visible, but MMIO mapping and queue setup are not enabled in the BIOS path' >&2
    exit 2
fi
if grep -Fq 'G6 virtio input transport DISCOVERED' "$log"; then
    echo 'BLOCKED: this QEMU virtio-keyboard-pci exposes modern-only PCI BARs to the BIOS path; queue completion was not claimed' >&2
    exit 2
fi
grep -Fq 'G6 virtio input transport READY' "$log"
grep -Fq 'G6 virtio input queue ARMED' "$log"
if grep -Fq 'G6 virtio input EVENT OK' "$log"; then
    echo 'PASS: real QEMU virtio-input queue completed an injected event'
else
    grep -Fq 'G6 virtio input EVENT WAIT' "$log"
    echo 'PASS: real QEMU virtio-input queue was armed; no host key event was injected'
fi

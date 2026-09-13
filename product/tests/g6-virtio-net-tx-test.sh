#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-virtio-net-tx.log"

# The kernel transaction and the Ring 3 boot fixture are both explicit test
# inputs. No host-side packet stub is involved: QEMU owns virtio-net-pci and
# its user-mode network backend consumes the submitted frame.
KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G6_VIRTIO_NET" \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -netdev user,id=n0 \
    -device virtio-net-pci,netdev=n0 \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'G6 virtio net transport READY' "$log"
grep -Fq 'G6 virtio net TX OK' "$log"
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: real QEMU virtio-net legacy queue 1 accepted a transmitted Ethernet frame'

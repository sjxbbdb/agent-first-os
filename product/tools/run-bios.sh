#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
image="${1:-$repo_root/build/bios/stage1.img}"
image_name="$(basename "${image%.*}")"
log="$repo_root/build/bios/qemu-$image_name.log"

if [[ ! -f "$image" ]]; then
    echo "missing image: $image; run build-bios.sh first" >&2
    exit 1
fi

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$log" \
    -machine pc \
    -drive "format=raw,file=$image,if=ide" \
    -boot c \
    -display none \
    -serial stdio \
    -monitor none
qemu_status=$?
set -e

if ! grep -Fq 'S1 BIOS OK - stage2 pending' "$log" || \
   ! grep -Fq 'S2 BIOS OK - stage2 loaded' "$log" || \
   ! grep -Fq 'S2 64BIT OK - long mode' "$log" || \
   ! grep -Fq 'KERNEL ELF OK - jumping to kernel' "$log" || \
   ! grep -Fq 'IDT OK - 256 vectors' "$log" || \
   ! grep -Fq 'KERNEL C OK' "$log"; then
    echo "boot chain/kernel marker not observed; see $log" >&2
    exit 1
fi

# Stage 1 intentionally halts forever; timeout is the expected termination.
if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi

cat "$log"
echo "verified BIOS boot chain, long mode, ELF handoff, and C kernel markers; QEMU timeout was expected"

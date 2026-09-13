#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

bash "$repo_root/product/tools/build-bios.sh"
cp "$build_dir/stage1.img" "$build_dir/stage1-bad-elf.img"
# Image layout: Stage 1 (1 sector) + fixed Stage 2 (24 sectors), then ELF.
printf '\0\0\0\0' | dd of="$build_dir/stage1-bad-elf.img" bs=1 seek=$((25 * 512)) conv=notrunc status=none

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$build_dir/qemu-bad-elf.log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1-bad-elf.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'S2 64BIT OK - long mode' "$build_dir/qemu-bad-elf.log"
grep -Fq 'S2 ERROR - invalid kernel ELF' "$build_dir/qemu-bad-elf.log"
if grep -Fq 'KERNEL ELF OK - jumping to kernel' "$build_dir/qemu-bad-elf.log" || \
   grep -Fq 'KERNEL C OK' "$build_dir/qemu-bad-elf.log"; then
    echo 'corrupt ELF unexpectedly reached kernel' >&2
    exit 1
fi

echo 'PASS: invalid ELF is diagnosed in Stage 2 and never reaches the kernel'

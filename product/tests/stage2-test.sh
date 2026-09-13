#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
stage1_source="$repo_root/product/kernel/arch/x86_64/boot/bios/stage1.asm"

bash "$repo_root/product/tools/build-bios.sh"

nasm -f bin -dSTAGE2_LBA=1048576 "$stage1_source" -o "$build_dir/stage1-bad-lba.bin"
cat "$build_dir/stage1-bad-lba.bin" "$build_dir/stage2.bin" > "$build_dir/stage1-bad-lba.img"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$build_dir/qemu-stage1-bad-lba.log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1-bad-lba.img,if=ide" \
    -boot c \
    -display none \
    -serial stdio \
    -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'S1 BIOS OK - stage2 pending' "$build_dir/qemu-stage1-bad-lba.log"
grep -Fq 'S1 BIOS disk read failed' "$build_dir/qemu-stage1-bad-lba.log"
if grep -Fq 'S2 BIOS OK - stage2 loaded' "$build_dir/qemu-stage1-bad-lba.log"; then
    echo 'bad LBA image unexpectedly reached Stage 2' >&2
    exit 1
fi

echo 'PASS: invalid Stage 2 LBA is retried, diagnosed, and never jumps to Stage 2'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

bash "$repo_root/product/tools/build-bios.sh"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$build_dir/qemu-no-long-mode.log" \
    -machine pc \
    -cpu qemu32 \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
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
grep -Fq 'S2 BIOS OK - stage2 loaded' "$build_dir/qemu-no-long-mode.log"
grep -Fq 'S2 ERROR - long mode unavailable' "$build_dir/qemu-no-long-mode.log"
if grep -Fq 'S2 64BIT OK - long mode' "$build_dir/qemu-no-long-mode.log"; then
    echo 'qemu32 unexpectedly entered long mode' >&2
    exit 1
fi

echo 'PASS: unsupported long mode is diagnosed and halted before mode switch'

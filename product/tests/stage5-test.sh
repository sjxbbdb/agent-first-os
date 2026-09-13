#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_DIV0 \
    bash "$repo_root/product/tools/build-bios.sh"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$build_dir/qemu-m5-div0.log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'IDT OK - 256 vectors' "$build_dir/qemu-m5-div0.log"
grep -Fq 'KERNEL C OK' "$build_dir/qemu-m5-div0.log"
grep -Fq 'EXCEPTION observed' "$build_dir/qemu-m5-div0.log"
grep -Fq 'vector=0x0000000000000000' "$build_dir/qemu-m5-div0.log"
grep -Fq 'EXCEPTION HALT' "$build_dir/qemu-m5-div0.log"

echo 'PASS: divide-by-zero reaches the initialized IDT, is logged, and halts'

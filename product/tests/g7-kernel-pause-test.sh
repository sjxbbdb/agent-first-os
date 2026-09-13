#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g7-kernel-pause.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_POLICY_PAUSE \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'SYSCALL policy token mint OK' "$log"
grep -Fq 'SYSCALL policy emergency pause OK' "$log"
grep -Fq 'SYSCALL policy token consume paused' "$log"
grep -Fq 'SYSCALL policy emergency resume OK' "$log"
grep -Fq 'SYSCALL policy token consume OK' "$log"
grep -Fq 'SYSCALL policy token consume timeout' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: Ring 3 policy authority pauses/resumes the kernel gate and expiry fails closed'

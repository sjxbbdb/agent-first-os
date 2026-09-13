#!/usr/bin/env bash
set -euo pipefail

# G2 kernel evidence: boot the real BIOS image with the Ring 3 fixture enabled
# and prove that two tasks have distinct roots, CR3 is loaded at launch and on
# a cooperative switch, then SYS_EXIT reclaims both tasks.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g2-kernel.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'G2 scheduler READY tasks=2' "$log"
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'G2 address spaces READY distinct CR3 roots' "$log"
grep -Fq 'G2 page tables PMM-backed identity-safe' "$log"
grep -Fq 'G2 CR3 switch OK' "$log"
grep -Fq 'USER RING3 OK' "$log"
grep -Fq 'SYSCALL exit OK - user reclaimed' "$log"
grep -Fq 'SCHEDULER switched task' "$log"
grep -Fq 'USER RING3 TASK2 OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"

echo 'PASS: kernel task lifecycle uses distinct CR3 roots, exits task 1, schedules task 2, and reaches idle'

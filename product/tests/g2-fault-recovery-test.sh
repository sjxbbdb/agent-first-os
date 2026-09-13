#!/usr/bin/env bash
set -euo pipefail

# G2 fault recovery evidence: task 1 takes a real user-mode #PF at an
# unmapped address. Ring 0 retires only that process and returns through the
# exception epilogue into task 2, which prints a marker and exits normally.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g2-fault-recovery.log"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_FAULT_RECOVERY" \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_FAULT_RECOVERY \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

if [[ "$qemu_status" -ne 124 && "$qemu_status" -ne 137 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'EXCEPTION observed' "$log"
grep -Fq 'vector=0x000000000000000e' "$log"
grep -Fq 'G2 user fault -> task zombie' "$log"
grep -Fq 'G2 fault recovery scheduled next task' "$log"
grep -Fq 'FAULT RECOVERY TASK2 OK' "$log"
grep -Fq 'SYSCALL exit OK - user reclaimed' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
if grep -Fq 'EXCEPTION HALT' "$log"; then
    echo 'FAIL: user fault halted the kernel instead of recovering task 2' >&2
    exit 1
fi
echo 'PASS: a Ring 3 page fault retires task 1 and resumes task 2'

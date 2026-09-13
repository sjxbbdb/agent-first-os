#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g2-wait-kill.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_WAIT_KILL \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]]
grep -Fq 'SYSCALL kill OK - child zombie' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'SYSCALL exit OK - user reclaimed' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
if grep -Fq 'USER RING3 TASK2 OK' "$log"; then
    echo 'FAIL: killed child ran after wait/kill' >&2
    exit 1
fi
echo 'PASS: child kill creates a zombie and wait reaps it before scheduler idle'

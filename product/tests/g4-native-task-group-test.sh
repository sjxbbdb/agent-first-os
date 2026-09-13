#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g4-task-group.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_TASK_GROUP \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]]
grep -Fq 'TASK GROUP RING3 START' "$log"
grep -Fq 'TASK GROUP CHILD RUN' "$log"
grep -Fq 'SYSCALL group terminate OK' "$log"
grep -Fq 'TASK GROUP TERMINATE OK' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'TASK GROUP CHILD REAPED' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native Ring 3 parent terminates and reaps its bounded task group'

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"; log="$build_dir/qemu-g4-group-freeze.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_GROUP_FREEZE \
 bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" -boot c -display none -serial stdio -monitor none
status=$?; set -e; [[ "$status" -eq 124 ]]
grep -Fq 'GROUP FREEZE RING3 START' "$log"
grep -Fq 'SYSCALL group freeze OK' "$log"
grep -Fq 'SYSCALL group resume OK' "$log"
grep -Fq 'GROUP FREEZE CHILD RUN' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native Ring 3 parent freezes and resumes its bounded task group'

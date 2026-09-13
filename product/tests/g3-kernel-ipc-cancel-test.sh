#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-ipc-cancel.log"

KERNEL_CFLAGS_EXTRA='-DAGENT_OS_TEST_RING3' \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_IPC_CANCEL \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]]
grep -Fq 'SYSCALL ipc recv blocked' "$log"
grep -Fq 'SYSCALL ipc cancel OK' "$log"
grep -Fq 'IPC CANCEL CHILD OK' "$log"
grep -Fq 'IPC CANCEL PARENT REAP OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
! grep -Fq 'IPC CANCEL FAIL' "$log"
! grep -Fq 'EXCEPTION observed' "$log"
printf 'PASS: parent cancels a blocked Ring 3 IPC wait and reaps the child\n'

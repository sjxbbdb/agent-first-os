#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-multiwait-ipc.log"

KERNEL_CFLAGS_EXTRA='-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_IPC_BLOCKING_MULTI' \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_IPC_BLOCKING_MULTI \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]]
grep -Fq 'G2 scheduler READY tasks=3' "$log"
[[ "$(grep -Fc 'SYSCALL ipc recv blocked' "$log")" -eq 2 ]]
[[ "$(grep -Fc 'SYSCALL ipc send wake OK' "$log")" -eq 2 ]]
grep -Fq 'IPC MULTI RECEIVER 1 OK' "$log"
grep -Fq 'IPC MULTI RECEIVER 2 OK' "$log"
grep -Fq 'IPC MULTI SENDER OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
! grep -Fq 'IPC MULTI FAIL' "$log"
! grep -Fq 'EXCEPTION observed' "$log"
printf 'PASS: two blocked Ring 3 receivers wake in order through one endpoint\n'

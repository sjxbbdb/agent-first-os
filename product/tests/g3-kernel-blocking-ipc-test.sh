#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-blocking-ipc.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_IPC_BLOCKING \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]]
grep -Fq 'SYSCALL ipc recv blocked' "$log"
grep -Fq 'SYSCALL ipc send wake OK' "$log"
grep -Fq 'IPC BLOCKING WAKE OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
! grep -Fq 'IPC BLOCKING FAIL' "$log"
! grep -Fq 'EXCEPTION observed' "$log"
printf 'PASS: Ring 3 blocking IPC parks, wakes, and copies the message\n'

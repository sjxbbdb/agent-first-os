#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-ipc-close.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_IPC_CLOSE \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]]
grep -Fq 'IPC CLOSE RING3 START' "$log"
grep -Fq 'SYSCALL ipc close OK' "$log"
grep -Fq 'IPC CLOSE SEND DENIED' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native Ring 3 endpoint close invalidates its capability generation'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-native-file-ipc.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G6_NATIVE \
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
grep -Fq 'SYSCALL ipc send OK' "$log"
grep -Fq 'SYSCALL ipc recv OK' "$log"
grep -Fq 'G6 NATIVE RING3 FILE IPC OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
printf 'PASS: isolated Ring 3 file service completes request/response through capability-checked IPC\n'

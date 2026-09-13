#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-native-dynamic-endpoint.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_DYNAMIC_ENDPOINT \
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
grep -Fq 'SYSCALL ipc create OK' "$log"
grep -Fq 'SYSCALL ipc destroy OK' "$log"
grep -Fq 'SYSCALL ipc send denied' "$log"
grep -Fq 'DYNAMIC IPC ENDPOINT LIFECYCLE OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native Ring 3 dynamic endpoint generation lifecycle'

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g3-shm.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_SHM \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 3s "$log" -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
[[ "$(grep -Fc 'SYSCALL shm map OK' "$log")" -eq 2 ]]
[[ "$(grep -Fc 'HM PING' "$log")" -eq 2 ]]
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: two Ring 3 processes map and observe the same capability-authorized shared page'

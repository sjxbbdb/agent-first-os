#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g4-native-recursive-group.log"

KERNEL_CFLAGS_EXTRA='-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_GROUP_RECURSIVE' \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_GROUP_RECURSIVE \
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
grep -Fq 'G2 scheduler READY tasks=3' "$log"
grep -Fq 'RECURSIVE GROUP CHILD RUN' "$log"
grep -Fq 'RECURSIVE GROUP GRANDCHILD RUN' "$log"
grep -Fq 'SYSCALL group terminate tree OK' "$log"
grep -Fq 'RECURSIVE GROUP TERMINATE OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native Ring 3 recursive task-group termination reaches a grandchild'

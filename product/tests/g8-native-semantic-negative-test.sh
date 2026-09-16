#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g8-native-semantic-negative.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 USER_NASMFLAGS_EXTRA='-DAGENT_OS_TEST_G8_NATIVE -DAGENT_OS_TEST_G8_NATIVE_NEGATIVE' bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]]
grep -Fq 'NATIVE REGISTRY NEGATIVE DENY OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: native semantic registry denies unsupported opcode and action digest in Ring 3'

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-native-file-write.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_FILE_WRITE \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
  -machine pc \
  -drive "format=raw,file=$build_dir/stage1.img,if=ide" -boot c -display none \
  -serial stdio -monitor none
status=$?
set -e
[[ "$status" -eq 124 ]]
grep -Fq 'SYSCALL policy token mint OK' "$log"
grep -Fq 'SYSCALL policy token consume OK' "$log"
grep -Fq 'SYSCALL policy token consume denied' "$log"
grep -Fq 'FILE_WRITE SUCCESS REPLAY_REJECTED SINGLE_WRITE' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
! grep -Fq 'FILE_WRITE FAIL' "$log"
printf 'PASS: native Ring3 file write token/replay/single-write slice\n'

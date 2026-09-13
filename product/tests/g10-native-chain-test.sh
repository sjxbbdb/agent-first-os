#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g10-native-chain.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G10_NATIVE \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" -machine pc \
  -drive "format=raw,file=$build_dir/stage1.img,if=ide" -boot c -display none -serial stdio -monitor none
status=$?
set -e
[[ "$status" -eq 124 ]]
for marker in 'CONTEXT COLLECTED' 'MOCK MODEL PLAN VALID' 'REGISTRY RESOLVED' \
  'JOURNAL PREPARE' 'JOURNAL COMMIT' 'JOURNAL REPLAY_REJECTED' \
  'JOURNAL ROLLBACK' 'ROLLBACK VERIFIED' 'G10 NATIVE CHAIN OK' 'SYSCALL policy token mint OK' \
  'SYSCALL policy token consume OK' 'SYSCALL policy token consume denied' \
  'SCHEDULER idle - all tasks exited'; do
  grep -Fq "$marker" "$log"
done
! grep -Fq 'EXCEPTION observed' "$log"
! grep -Fq 'G10 SERVICE FAIL' "$log"
printf 'PASS: bounded native G10 mock-model IPC/policy/file journal chain\n'

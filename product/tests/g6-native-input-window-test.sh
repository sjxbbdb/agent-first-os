#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-native-input-window.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_G6_NATIVE_IO \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" -machine pc \
  -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -boot c -display none -serial stdio -monitor none
status=$?
set -e
[[ "$status" -eq 124 ]]
grep -Fq 'G6 NATIVE NET INPUT OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
! grep -Fq 'G6 NATIVE IO SERVICE FAIL' "$log"
echo 'PASS: synthetic Ring3 input IPC vertical slice; window boundary remains protocol-only'

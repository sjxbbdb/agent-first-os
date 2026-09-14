#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g9-runtime-service.log"
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_RUNTIME_SERVICE \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 15s "$log" \
  -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -boot c -display none -serial stdio -monitor none
status=$?
set -e
[[ "$status" -eq 124 ]] || { cat "$log"; exit 1; }
for marker in 'AGENT RUNTIME SERVICE START' 'AGENT RUNTIME HEARTBEAT ACK' \
  'AGENT RUNTIME CHECKPOINT SENT' 'AGENT RUNTIME SERVICE ABI OK' \
  'SCHEDULER idle - all tasks exited'; do
  grep -Fq "$marker" "$log"
done
! grep -Fq 'AGENT RUNTIME SERVICE ABI FAIL' "$log"
echo 'PASS: BIOS/QEMU Ring 3 Runtime START/heartbeat/checkpoint IPC slice'

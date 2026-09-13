#!/usr/bin/env bash
set -euo pipefail

# G4 native evidence: Ring 0 starts two ordinary Ring 3 processes.  The first
# is a Supervisor fixture and the second is its service.  The Supervisor
# yields, receives a heartbeat, restarts the parent-owned zombie service,
# receives the second heartbeat, then waits/reaps it.  No Supervisor policy
# code runs in the kernel; Ring 0 only validates lifecycle authority.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g4-native-supervisor.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_SUPERVISOR \
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
grep -Fq 'SUPERVISOR RING3 START' "$log"
grep -Fq 'SUPERVISOR HEARTBEAT 1' "$log"
grep -Fq 'SYSCALL restart OK - service READY' "$log"
grep -Fq 'SUPERVISOR RESTART HEARTBEAT 2' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'SUPERVISOR SERVICE EXIT REAPED' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
[[ "$(grep -Fc 'SYSCALL ipc send OK' "$log")" -eq 2 ]]
[[ "$(grep -Fc 'SYSCALL exit OK - user reclaimed' "$log")" -eq 3 ]]
echo 'PASS: native Ring 3 Supervisor receives heartbeat, restarts a service, and reaps its exit'

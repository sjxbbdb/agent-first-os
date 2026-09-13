#!/usr/bin/env bash
set -euo pipefail

# G4 native fault-restart evidence.  The Ring 3 service sends one heartbeat,
# takes a real user-mode #PF, and becomes a zombie.  Its Ring 3 Supervisor
# restarts the parent-owned process through SYS_RESTART, receives the second
# heartbeat, and reaps the clean exit through SYS_WAIT.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g4-supervisor-fault-restart.log"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_SUPERVISOR_FAULT" \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_SUPERVISOR_FAULT \
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
grep -Fq 'SUPERVISOR FAULT RING3 START' "$log"
grep -Fq 'SERVICE #PF CRASH INJECTED' "$log"
grep -Fq 'EXCEPTION observed' "$log"
grep -Fq 'vector=0x000000000000000e' "$log"
grep -Fq 'G4 Supervisor service #PF -> zombie' "$log"
grep -Fq 'SUPERVISOR FAULT HEARTBEAT 1' "$log"
grep -Fq 'SYSCALL restart OK - service READY' "$log"
grep -Fq 'SERVICE RESTARTED AFTER #PF' "$log"
grep -Fq 'SUPERVISOR FAULT RESTART HEARTBEAT 2' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
grep -Fq 'SUPERVISOR FAULT SERVICE REAPED' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
[[ "$(grep -Fc 'SYSCALL exit OK - user reclaimed' "$log")" -eq 2 ]]
if grep -Fq 'EXCEPTION HALT' "$log"; then
    echo 'FAIL: Supervisor fault recovery halted the kernel' >&2
    exit 1
fi
echo 'PASS: Ring 3 Supervisor restarts a user-faulted service and reaps it'

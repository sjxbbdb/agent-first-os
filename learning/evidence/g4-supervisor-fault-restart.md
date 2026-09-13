# G4 Supervisor service fault/restart evidence

## Scope

This fixture is a native BIOS/QEMU vertical slice. A Ring 3 service sends a
heartbeat, touches an unmapped user address, and takes a real page fault. The
Ring 0 exception path retires only that process as a zombie and schedules the
Ring 3 Supervisor. The Supervisor invokes the parent-checked `SYS_RESTART`,
receives the restarted service heartbeat, and invokes `SYS_WAIT` to reap its
clean exit.

## Reproduction

```text
bash product/tests/g4-supervisor-fault-restart-test.sh
```

The test builds with `AGENT_OS_TEST_SUPERVISOR_FAULT`, runs `stage1.img` under
SeaBIOS/QEMU, and writes the serial trace to
`build/bios/qemu-g4-supervisor-fault-restart.log`.

## Required serial markers

```text
SUPERVISOR FAULT RING3 START
SERVICE #PF CRASH INJECTED
EXCEPTION observed
vector=0x000000000000000e
G4 Supervisor service #PF -> zombie
SYSCALL restart OK - service READY
SERVICE RESTARTED AFTER #PF
SUPERVISOR FAULT RESTART HEARTBEAT 2
SYSCALL wait OK - child reaped
SUPERVISOR FAULT SERVICE REAPED
SCHEDULER idle - all tasks exited
```

## Boundary

This proves native process fault retirement, parent-owned restart, heartbeat
continuity, and wait/reap. It does not prove production service manifests,
restart backoff limits, persistent journal recovery, or real hardware fault
delivery. The macro is fixture-only and remains outside the default build.

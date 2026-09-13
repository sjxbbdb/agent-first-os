# G4 native Ring 3 Supervisor vertical slice

## Scope

`g4-native-supervisor-test.sh` boots the real SeaBIOS image in QEMU with
`AGENT_OS_TEST_RING3` and `AGENT_OS_TEST_SUPERVISOR`.  Ring 0 creates two
ordinary Ring 3 processes: the first is a Supervisor fixture and the second
is its service.  The Supervisor owns the lifecycle sequence through the
documented syscall/IPC ABI:

1. `SYS_YIELD` lets the service run.
2. The service sends an inline heartbeat over the endpoint capability and
   exits.
3. The Supervisor receives the heartbeat and calls `SYS_RESTART` for its own
   zombie child.
4. The restarted service sends a second heartbeat and exits.
5. The Supervisor calls `SYS_WAIT`, reaps the service, and exits.

`SYS_RESTART` is intentionally narrow: Ring 0 accepts only a zombie whose
`parent` is the current process, restores the recorded initial user frame, and
returns it to `READY`.  It is not a general process-creation or policy API.
No Supervisor state machine or model code is linked into the kernel.

## Command and observed evidence

```text
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_SUPERVISOR \
  bash product/tools/build-bios.sh
bash product/tests/g4-native-supervisor-test.sh
```

The QEMU serial log contains, in order:

```text
SUPERVISOR RING3 START
SYSCALL ipc send OK
SERVICE HEARTBEAT SENT
SYSCALL exit OK - user reclaimed
SYSCALL ipc recv OK
SUPERVISOR HEARTBEAT 1
SYSCALL restart OK - service READY
SYSCALL ipc send OK
SERVICE HEARTBEAT SENT
SYSCALL exit OK - user reclaimed
SYSCALL ipc recv OK
SUPERVISOR RESTART HEARTBEAT 2
SYSCALL wait OK - child reaped
SUPERVISOR SERVICE EXIT REAPED
SCHEDULER idle - all tasks exited
```

The test also asserts exactly two service sends and three process exits (two
service generations plus the Supervisor), and requires QEMU to run until the
bounded timeout after reaching scheduler idle.

## Remaining G4 boundary

This is a native lifecycle proof, not the complete initrd Supervisor gate.
The service is embedded in the BIOS fixture, capabilities are statically
minted, and the restart path restores one fixed process frame.  Manifest
loading, dependency DAG enforcement in Ring 3, task-group capability
revocation/freeze, crash collection, and a general service spawn/recovery
protocol remain to be integrated before G4 can be marked complete.

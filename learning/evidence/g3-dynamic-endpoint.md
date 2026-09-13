# G3 dynamic endpoint lifecycle

## Green evidence

The bounded endpoint slice now owns endpoint storage in a Ring-0 pool. The
kernel mints a generation-tagged capability only after the caller proves the
kernel-provisioned policy-admin capability. `SYS_IPC_CALL`, `SYS_IPC_RECV`,
and `SYS_IPC_RECV_WAIT` resolve the capability to an active pool slot instead
of comparing against one fixed address. Close wakes waiters with
`ECANCELED`; destroy closes the endpoint, retires the caller's capability
generation, and makes later use fail closed.

The earlier red probe (`g3_dynamic-endpoint-red-test.sh`) is retained as a
historical contract check; it now compiles because the create-in-place Ring-0
primitive exists. The green lifecycle checks below are the authoritative
acceptance tests.

Host evidence:

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g3-dynamic-endpoint-test.sh"
dynamic endpoint create/close/destroy/reuse: PASS
PASS: bounded endpoint object lifecycle rejects closed and destroyed use
```

Native BIOS/QEMU evidence:

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g3-native-dynamic-endpoint-test.sh"
SYSCALL ipc create OK
SYSCALL ipc close OK
SYSCALL ipc recv wait closed
SYSCALL ipc destroy OK
SYSCALL ipc send denied
DYNAMIC IPC ENDPOINT LIFECYCLE OK
SCHEDULER idle - all tasks exited
PASS: native Ring 3 dynamic endpoint generation lifecycle
```

The native fixture creates an endpoint from Ring 3, sends and receives a
bounded message through its returned capability, closes it, proves an empty
closed endpoint returns immediately from `SYS_IPC_RECV_WAIT`, destroys it, and
proves the old capability cannot send afterward. The host fixture also proves
close, destroy, and safe slot reuse.

## Remaining boundary

This is a bounded four-slot kernel pool. It does not yet provide a general
allocator, endpoint naming/service discovery, reply objects, timeout values,
or persistent object accounting. Capability lineage and waiter cancellation
remain bounded by the existing process table; no claim is made for SMP or
real hardware concurrency.

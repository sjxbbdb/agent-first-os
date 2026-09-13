# G4 clean-exit capability re-mint

## Scope

Clean `SYS_EXIT` now snapshots a bounded list of the exiting process' object/
rights descriptors, revokes its live capability entries (advancing each
generation), and leaves the zombie restartable. The existing parent-only
`SYS_RESTART` path re-mints fresh handles from that snapshot. A parent can
still transfer a newly authorized capability after restart; stale handles do
not regain access.

## Red/green

Before this slice, `SYS_EXIT` called `agent_os_process_exit` directly, which
left cleanly exited processes' capabilities active across restart. The host
test specifically asserts that the pre-exit handle fails after clean exit and
after restart, then transfers a fresh handle from the parent:

```text
G4 clean-exit restart capability re-mint OK
```

The targeted native Ring 3 Supervisor test also passed after the service's
restart frame receives the newly minted endpoint handle:

```text
PASS: native Ring 3 Supervisor receives heartbeat, restarts a service, and reaps its exit
```

Its QEMU log contains two successful IPC sends, restart READY, second
heartbeat, wait/reap, and scheduler idle markers.

## Limits

This is a bounded fixed-table lifecycle slice. It re-mints at most 16 saved
descriptors, does not persist capabilities across kernel restart, does not
implement general process creation or service discovery, and does not claim a
complete capability garbage collector. Fault-to-restart remains separately
scoped from clean-exit re-mint.

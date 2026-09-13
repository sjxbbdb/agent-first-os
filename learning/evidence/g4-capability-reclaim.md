# G4 process capability reclaim

Ring 0 now revokes every active capability owned by a process as part of
`agent_os_process_kill`, which is the destructive parent-authorized lifecycle
path.  Ordinary clean exit remains restart-compatible for the existing G4
service fixture; a follow-up must define capability re-minting before applying
reclaim to restartable clean exits.
Each slot is deactivated and its generation advances, so stale handles fail
the normal capability lookup generation check.  Descendant tables and shared
endpoint objects are not implicitly destroyed.

Host evidence:

```text
bash product/tests/g4-cap-reclaim-test.sh
G4 process capability reclaim OK
```

The existing BIOS/QEMU lifecycle regression remains green after this kernel
change:

```text
bash product/tests/g4-native-supervisor-test.sh
PASS: native Ring 3 Supervisor receives heartbeat, restarts a service, and reaps its exit
```

The native fixture does not yet expose a stale-handle probe after process exit;
that remains a follow-up for an explicit QEMU capability-reclaim assertion.

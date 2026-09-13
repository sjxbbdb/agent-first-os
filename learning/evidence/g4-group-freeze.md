# G4 task-group freeze/resume slice

## Implemented kernel contract

Ring 0 now carries `group_id` and `AGENT_OS_PROCESS_FROZEN`.  `SYS_GROUP_FREEZE`
and `SYS_GROUP_RESUME` accept a group id only from the live parent process and
transition matching direct children between `READY` and `FROZEN`.  The scheduler
only selects `READY`, so a frozen child cannot run.  Descendants, unrelated
groups, and the caller are not affected.

## Evidence status

The host lifecycle test passes:

```text
bash product/tests/g4-group-freeze-test.sh
G4 task-group freeze/resume OK
```

The native BIOS/QEMU fixture was rerun after the transient image lock cleared:

```text
bash product/tests/g4-native-group-freeze-test.sh
PASS: native Ring 3 parent freezes and resumes its bounded task group
```

The serial assertions require `group freeze OK`, `group resume OK`, child
execution, wait/reap, and scheduler idle.

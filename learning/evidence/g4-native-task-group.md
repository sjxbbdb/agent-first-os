# G4 native Ring 3 task-group lifecycle slice

## Scope

Ring 0 assigns each process an opaque `group_id` at creation.  `SYS_GROUP_TERMINATE`
accepts a group id and exit code, but only the live parent may invoke it; the
kernel transitions matching direct children to zombies.  Unrelated groups,
descendants, and the caller are excluded.  `SYS_WAIT` then reaps the child.

This is a bounded lifecycle slice.  It does not implement recursive groups,
freeze/resume, or capability revocation, and the host Supervisor reference
remains a separate control-plane model.

## Native BIOS/QEMU evidence

```text
bash product/tests/g4-native-task-group-test.sh
PASS: native Ring 3 parent terminates and reaps its bounded task group
```

The test builds the BIOS image with `AGENT_OS_TEST_RING3` and
`AGENT_OS_TEST_TASK_GROUP`, then boots the real SeaBIOS image in QEMU.  The
serial log includes:

```text
TASK GROUP RING3 START
TASK GROUP CHILD RUN
SYSCALL group terminate OK
TASK GROUP TERMINATE OK
SYSCALL wait OK - child reaped
TASK GROUP CHILD REAPED
SCHEDULER idle - all tasks exited
```

The host unit test is supplemental only:

```text
bash product/tests/g4-task-group-test.sh
G4 native task-group lifecycle OK
```

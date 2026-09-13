# G4 native recursive task group

The BIOS/QEMU fixture creates a Ring 3 parent, a direct child, and a
grandchild. Both descendants share group id 9. The parent yields once so the
two descendants enter the scheduler, then calls `SYS_GROUP_TERMINATE_TREE`.
The kernel follows parent links, terminates both matching descendants, and
the parent reaps its direct child.

Evidence command:

```text
bash product/tests/g4-native-recursive-task-group-test.sh
```

Expected green markers:

```text
G2 scheduler READY tasks=3
RECURSIVE GROUP CHILD RUN
RECURSIVE GROUP GRANDCHILD RUN
SYSCALL group terminate tree OK
RECURSIVE GROUP TERMINATE OK
SCHEDULER idle - all tasks exited
```

The traversal is bounded by `AGENT_OS_MAX_PROCESSES` and remains caller-scoped;
unrelated groups and ancestors are not reachable. Capability reaping on clean
exit, recursive freeze/resume, and general process creation remain separate
work.

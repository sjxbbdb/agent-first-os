# G2 fault-to-task recovery evidence

The BIOS/QEMU fixture builds two independent Ring 3 processes. Task 1 reads
the unmapped user virtual address `0x00300000`, producing a real x86 page fault
(vector 14). The Ring 0 exception path marks only the current process as a
zombie, selects the next READY process, loads its address space, replaces the
saved return frame, and returns through `iretq`. Task 2 prints
`FAULT RECOVERY TASK2 OK` and exits.

Command:

```text
bash product/tests/g2-fault-recovery-test.sh
```

Expected evidence markers:

```text
EXCEPTION observed
vector=0x000000000000000e
G2 user fault -> task zombie
G2 fault recovery scheduled next task
FAULT RECOVERY TASK2 OK
SCHEDULER idle - all tasks exited
```

This is a bounded single-core BIOS/QEMU slice. It does not yet provide demand
paging, a general fault policy, restart of the failed task, or hardware
coverage beyond the QEMU machine used by the test.

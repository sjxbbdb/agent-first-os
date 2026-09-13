# G3 native blocking IPC

`product/tests/g3-kernel-blocking-ipc-test.sh` boots two Ring 3 fixtures. The
first calls the dedicated `SYS_IPC_RECV_WAIT` ABI with an empty endpoint. Ring
0 marks it `BLOCKED` and schedules the second task. The second task sends the
message through the capability-checked endpoint; Ring 0 copies the bounded
message into the waiting task's user buffer while its address space is active,
wakes it, and the first task verifies the opcode and payload before both tasks
reach scheduler idle.

The original non-blocking `SYS_IPC_RECV` behavior remains unchanged. This is a
single-endpoint, single-waiter BIOS/QEMU slice; dynamic endpoint allocation,
multiple waiters, cancellation, and reply objects remain future work.

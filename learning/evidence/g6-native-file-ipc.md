# G6 native Ring 3 file IPC evidence

Command:

```text
bash product/tests/g6-native-file-ipc-test.sh
```

The test builds the real BIOS image with `AGENT_OS_TEST_RING3` and the
`AGENT_OS_TEST_G6_NATIVE` user fixture, boots it in QEMU/SeaBIOS, and records
the serial output in `build/bios/qemu-g6-native-file-ipc.log`.

Expected markers are `SYSCALL ipc send OK`, `SYSCALL ipc recv OK`,
`G6 NATIVE RING3 FILE IPC OK`, and `SCHEDULER idle - all tasks exited`.
The client and service are separate Ring 3 address spaces. The kernel checks
the opaque endpoint capability and user receive buffer, then only transports
the bounded message. File naming and result semantics remain a Ring 3 fixture;
there is no virtio filesystem driver, persistence, or generic service launch.

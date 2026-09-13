# G6 native network/input service IPC interface

`product/tests/g6-native-io-test.sh` boots the BIOS/QEMU image with two Ring 3
fixtures. The Agent sends a bounded network request (`0xE8`), the service
returns a network completion marker, then the Agent sends an input poll request
(`0xEA`) and receives an input completion marker. Both exchanges use the real
capability-checked `SYS_IPC_CALL`/`SYS_IPC_RECV` ABI and reach scheduler idle.

This proves the native userland service interface and IPC ordering. The
responses are fixed fixture markers; they do not claim a virtio-net packet,
virtio-input event queue, window system, or physical device backend.

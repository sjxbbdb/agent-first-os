# G6 virtio-net RX queue evidence

The gated RX transaction configures a queue-0 device-write descriptor, publishes
it in the available ring, notifies QEMU, and polls the device-visible used
index. `RX OK` is emitted only after `used.idx` advances. With QEMU's user
network backend and no injected host packet, `RX WAIT` is a bounded queue-arm
result and the focused test exits 2; it does not claim RX completion.

The focused test also supports a controlled local socket backend: it starts
`g6_qemu_packet_injector.py`, connects QEMU with `-netdev socket`, and sends a
deterministic Ethernet frame. QEMU's TCP socket backend uses a four-byte
big-endian packet-length prefix; the injector therefore prefixes each frame and
pads it to the 60-byte Ethernet minimum. It repeats the bounded send briefly so
the guest can arm queue 0 after the socket connects. The test accepts success
only when the guest observes `used.idx` and emits `G6 virtio net RX OK`; injector
delivery alone is not evidence.

The native handoff ABI is now fixed in `userland/include/service_protocol.h` as
`AgentOsVirtioRxBuffer` (versioned header, queue/descriptor, opaque capability
reference, generation, address, and bounded length). The ABI layout test is
host-only; the kernel still does not produce this record or deliver it over
IPC, so native Ring 3 handoff remains blocked.

Run:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-net-rx-test.sh'
```

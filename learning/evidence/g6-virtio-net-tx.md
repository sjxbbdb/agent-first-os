# G6 legacy virtio-net TX queue evidence

`product/kernel/src/virtio_pci.c` now configures both legacy virtio-net queue
0 and queue 1 before setting `DRIVER_OK`. The bounded transaction creates a
10-byte legacy virtio-net header plus a 60-byte Ethernet frame, publishes one
descriptor chain in queue 1, kicks the device, and waits for the used index to
advance. The kernel emits `G6 virtio net TX OK` only after that device-visible
completion.

The transaction is gated by `AGENT_OS_TEST_G6_VIRTIO_NET` in `KERNEL_CFLAGS_EXTRA`.
The same command enables the existing `AGENT_OS_TEST_RING3` user fixture so the
test also proves the normal boot path after the device transaction. The QEMU
backend is `-netdev user -device virtio-net-pci`; no fixed host response or
host-only service fixture is used as evidence.

Verification command:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-net-tx-test.sh'
```

Expected evidence:

```text
PASS: real QEMU virtio-net legacy queue 1 accepted a transmitted Ethernet frame
```

This is one bounded transmit transaction. It does not claim RX packet
delivery, interrupt-driven operation, checksum/feature negotiation, modern
virtio transport, input events, or a complete network service.

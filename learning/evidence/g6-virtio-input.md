# G6 virtio-input queue evidence

`product/kernel/src/virtio_pci.c` now has two bounded virtio-input paths. The
modern path walks the PCI capability chain, maps only the validated PCI MMIO
aperture into supervisor-only 4 KiB page tables, negotiates zero driver
features, selects queue 0, publishes descriptor/avail/used addresses, enables
the queue, and writes the notify doorbell. The legacy path remains available.
`EVENT OK` is printed only when used index advances and the eight-byte event
buffer is read.

The transaction is gated by `AGENT_OS_TEST_G6_VIRTIO_INPUT` in
`KERNEL_CFLAGS_EXTRA`. The test accepts `EVENT WAIT` when QEMU receives no host
keyboard input; this is queue-arm evidence, not an input-event completion.
The focused fixture uses QEMU's default modern-only `virtio-keyboard-pci` and
records `G6 virtio input transport READY` plus `G6 virtio input queue ARMED`.
This proves queue setup and descriptor publication. It does not prove an input
event until a host key reaches the device and `used.idx` advances.

Run the focused check from WSL:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-input-test.sh'
```

On the current QEMU build, the focused test passes with:

```text
PASS: real QEMU virtio-input queue was armed; no host key event was injected
```

The separate QMP test sends a key press/release, but remains BLOCKED because
no guest `used.idx` completion is observed. The remaining boundary is
host-event delivery or polling timing, rather than BAR mapping or queue
initialization. Common and notify are the minimum capability proof; device
config remains an optional recorded address.

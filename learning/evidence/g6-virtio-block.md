# G6 virtio block transport evidence

`product/kernel/src/virtio_pci.c` is the first hardware-facing G6 slice. It
scans PCI configuration space for transitional virtio block and network IDs,
resets each device, acknowledges/negotiates an empty feature set, selects
queue 0, clears an aligned queue page, publishes its PFN, and reaches
`DRIVER_OK`. For virtio-blk it submits a three-descriptor read request for
sector 0 and validates the returned 0x55AA boot signature. It also discovers the modern virtio-input PCI ID so the input
service boundary is visible even though its modern common-configuration queue
is not initialized yet. The kernel only owns this transport boundary; file
operations remain a future Ring 3 service interface.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-block-test.sh'
PASS: QEMU virtio-blk PCI legacy transport initializes queue 0, reads sector 0, and preserves Ring 3 boot
```

The test adds QEMU `virtio-blk-pci`, `virtio-net-pci`, and
`virtio-keyboard-pci` devices and still reaches the normal Ring 3 ELF fixture
and scheduler idle. It does not yet submit a network request or initialize
modern input queues. The separate host-only service fixture can be run with:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-service-test.sh'
G6 Ring3 file/network/input/window service fixture: PASS
```

That fixture validates the userland protocol shape, bounded payloads,
capability binding/revocation, duplicate request rejection, and file/network/
input/window semantics. It is not evidence of a native Ring 3 process, a full
virtio network/input queue, or real hardware support; those remain open G6
work.

# G6 virtio-input queue evidence

`product/kernel/src/virtio_pci.c` now has a bounded legacy virtio-input
transaction for QEMU `virtio-keyboard-pci,disable-modern=on`: after PCI discovery and feature
negotiation it configures queue 0, publishes one device-write event descriptor,
notifies the device, and polls the device-visible used index. `EVENT OK` is
printed only when used index advances and the eight-byte event buffer is read.

The transaction is gated by `AGENT_OS_TEST_G6_VIRTIO_INPUT` in
`KERNEL_CFLAGS_EXTRA`. The test accepts `EVENT WAIT` when QEMU receives no host
keyboard input; this is queue-arm evidence, not an input-event completion.
The legacy path is covered here. QEMU's default device exposes modern-only
BARs to this BIOS image, so the focused fixture explicitly selects the
transitional legacy transport. Modern virtio-input PCI capability/common
configuration and reliable QMP key injection remain unverified.

Run the focused check from WSL:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-input-test.sh'
```

On the current QEMU build, the default device was used, and the guest reported
`G6 virtio input transport DISCOVERED`: no legacy I/O BAR was exposed to this
BIOS path. The focused test therefore exits with status 2 and records:

```text
BLOCKED: this QEMU virtio-keyboard-pci exposes modern-only PCI BARs to the BIOS path; queue completion was not claimed
```

The source path contains the bounded legacy queue arm/poll implementation, but
there is no QEMU runtime evidence for it until a legacy-capable input model or
the modern PCI common configuration/MMIO path is added. The probe now audits
the modern PCI capability chain and records common/notify/device configuration
addresses without dereferencing unmapped MMIO. This narrows the blocker but is
still discovery evidence only; no PASS is claimed. Common and notify are the
minimum capability proof; device config remains an optional recorded address.

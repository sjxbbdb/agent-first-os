# G6 modern virtio-input BAR boundary

The kernel applies a page-granular, supervisor-only identity map before modern
virtio common-config MMIO access. The mapping helper accepts only the bounded
QEMU PCI aperture (`0xfd000000..0xff000000`), rejects overflow and 2 MiB
crossing, and never sets the user bit. The pure range helper remains covered by
`g6-virtio-input-bar-contract-test.sh`; the focused QEMU test additionally proves
that the discovered modern BARs can be mapped and queue 0 can be armed. This is
queue-setup evidence, not input-event completion or physical-hardware evidence.

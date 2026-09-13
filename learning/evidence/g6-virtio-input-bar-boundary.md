# G6 modern virtio-input BAR boundary

The kernel now applies a pure range check before any future modern virtio
common-config MMIO access. A BAR window must be non-empty, start below the
current `0x200000` identity-map end, and fit without overflow. The helper is
covered by `g6-virtio-input-bar-contract-test.sh` for a valid low range,
identity-map crossing, and high-address overflow. Current QEMU modern BARs are
still not mapped into this BIOS identity window, so this is a safety contract,
not queue or event completion evidence.

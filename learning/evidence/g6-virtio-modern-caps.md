# G6 modern virtio capability layout contract

`g6-virtio-modern-caps-test.sh` runs a host-side parser contract over a small
PCI configuration capability fixture. It accepts common and notify
capabilities, and rejects invalid BAR indexes, backward/cyclic next pointers,
truncated capability records, and offset-plus-length overflow. This is a
malformed-layout safety check only: it does not map or dereference a PCI BAR,
enable a virtqueue, observe used.idx, or claim a real QEMU input event.

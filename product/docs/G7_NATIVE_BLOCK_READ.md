# G7 native virtio block read slice

`SYS_VIRTIO_BLOCK_READ` is a test-gated Ring 3 adapter for one 512-byte data
sector. Ring 0 requires an opaque device capability with `CAP_RIGHT_READ`, a
nonzero sector, ABI version 1, exact length 512, and a writable user range
before submitting the read. The completed data is copied from the kernel-owned
request page into the user destination.

The BIOS/QEMU fixture rejects bad ABI, sector zero, forged capabilities and an
unmapped destination, then reads a deterministic payload from sector 1 of an
independent virtio disk. This is a transport/service primitive only; it does
not provide a journal scanner, transaction semantics or crash recovery.

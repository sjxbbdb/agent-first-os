# G7 native virtio block write slice

`SYS_VIRTIO_BLOCK_WRITE` is a versioned, test-gated Ring 3 adapter for one
512-byte write. The ABI passes an opaque device capability in `RDI`, a data
sector in `RSI`, a user write buffer in `RDX`, the exact length in `R10`, and
the ABI version in `R8`. Ring 0 checks the capability and `CAP_RIGHT_WRITE`,
rejects sector zero, requires version 1 and exactly 512 bytes, validates the
current process's user range, and only then calls
`agent_os_virtio_block_write_sector()`.

The bootstrap grant and assembly client are compiled only with
`AGENT_OS_TEST_G7_NATIVE_BLOCK_WRITE`. The BIOS/QEMU test attaches a separate
virtio data disk, waits for the serial `used completion` marker, and checks
that sector 1 contains the fixture payload.

This is a bounded transport/service vertical slice. It does not provide a
general block service, durable transaction journal, crash persistence,
recovery, flush/barrier semantics, or hardware DMA/IOMMU isolation.

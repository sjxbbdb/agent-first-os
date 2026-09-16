# G7 native virtio-block flush slice

`SYS_VIRTIO_BLOCK_FLUSH` is a test-gated Ring 3 adapter for one
`VIRTIO_BLK_T_FLUSH` request. The legacy block probe reads and negotiates
`VIRTIO_BLK_F_FLUSH` (`1 << 9`) and records the result in
`AgentOsVirtioProbe.flush_supported`. Ring 0 accepts only the opaque device
capability with `CAP_RIGHT_WRITE` and ABI version 1; unsupported, forged, or
wrong-version requests return `-EOPNOTSUPP`.

The QEMU fixture attaches an independent virtio disk, verifies the negotiated
feature, submits a two-descriptor no-data request, and waits for the used-ring
completion. The slice does not claim journaling, recovery, or general
persistence semantics.

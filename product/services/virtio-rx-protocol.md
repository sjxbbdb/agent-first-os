# G6 virtio-net RX buffer handoff ABI v1

The Ring 3 network service receives a versioned descriptor after Ring 0 has
completed an RX queue entry. Fields are `version=1`, `queue=0`, descriptor
index, capability handle plus capability generation, guest buffer address, and
bounded Ethernet buffer length (14..1514 bytes). The service must reject a
wrong queue, stale generation, empty handle, non-integral values, or lengths
outside the Ethernet frame bound.

This is a service contract and host validation only. It does not claim native
virtio completion, DMA/IOMMU validation, or a Ring 3 QEMU handoff.

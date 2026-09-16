# G7 native journal recovery slice

This fixture keeps journal interpretation in Ring 3 and uses only the
capability-checked block read, block write and flush primitives. The prepare
run writes a fixed PREPARE record to sector 1, flushes it, writes a new value
to payload sector 3, flushes it, then stays in a controlled crash loop before
COMMIT. The recovery run scans sector 1, verifies the PREPARE marker and the
new payload, restores the old payload, and appends a rollback marker to sector
1 with flush completion.

`g7-native-journal-recovery-test.sh` uses the same `cache=directsync` data
image for both QEMU runs and stops the first run through QMP after the durable
markers appear. Host checks verify the rollback record and restored payload.
This is BIOS/QEMU crash-restart evidence for one fixed fixture; it is not a
general journal format, full commit/replay matrix, filesystem, physical power
loss guarantee, DMA/IOMMU isolation or production durability claim.

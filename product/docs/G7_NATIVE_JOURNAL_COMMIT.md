# G7 native journal commit recovery slice

This fixture complements the rollback test with the other durable state
transition. Ring 3 writes a fixed `PREPARE` record, flushes a new payload,
writes and flushes a `COMMIT` record, then stays in a controlled crash loop.
The recovery run reads the same data disk, verifies the commit record and new
payload, writes an `APPLIED1` marker, and flushes without restoring the old
value.

`g7-native-journal-commit-recovery-test.sh` uses QEMU `cache=directsync`, QMP
termination, serial markers, and host sector checks. It proves one committed
transaction survives a controlled restart. It is not a general journal,
multi-transaction replay engine, corruption matrix, filesystem, physical power
loss guarantee, DMA/IOMMU isolation or production durability claim.

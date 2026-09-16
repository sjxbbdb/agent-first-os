# G7 native journal write boundary

`agent_os_virtio_block_write_sector()` adds the Ring 0 hardware primitive for a
single validated 512-byte legacy virtio-blk write. It uses three descriptors,
checks device status, and polls a bounded completion. The primitive accepts a
sector number from its caller; sector-zero policy belongs to the Ring 3 service
and its authorization path.

`product/tests/g7-native-journal-contract-test.sh` compiles the actual
transport source and checks the data-sector boundary. This is a source and ABI
contract only. A native syscall/IPC adapter, journal format, crash recovery,
and persistence across reboot are not implemented or claimed yet.

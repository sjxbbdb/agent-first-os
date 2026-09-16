# G8 native semantic registry negative evidence

The BIOS/QEMU fixture sends unsupported semantic opcode `0x7f` and invalid action digest `0xbadac71000000001`. The Ring 3 registry service emits `0x9f` deny replies with zero payload for both. The IPC ABI header remains version 1; this test does not claim forged-header-version handling. Ring 0 only transports bounded IPC and does not interpret registry semantics.

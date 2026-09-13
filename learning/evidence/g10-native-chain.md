# G10 native BIOS/QEMU composite fixture

`product/tests/g10-native-chain-test.sh` builds and boots the bounded
`AGENT_OS_TEST_G10_NATIVE` fixture through SeaBIOS/QEMU. Ring 3 bootstrap emits
`CONTEXT COLLECTED`, `MOCK MODEL PLAN VALID`, and `REGISTRY RESOLVED`, then uses
the existing `SYS_IPC_CALL`/`SYS_IPC_RECV` path to ask the Ring 3 policy/service
for a one shot token. The service consumes that token once, writes a fixed
postcondition marker into its own writable Ring 3 stack buffer, and returns it.
It emits `JOURNAL PREPARE` and `JOURNAL COMMIT`; replay is rejected and emits
`JOURNAL REPLAY_REJECTED`. A second fixed action intentionally fails its
postcondition, restores the in-memory preimage, and emits `JOURNAL ROLLBACK`
and `ROLLBACK VERIFIED`. The test checks the complete marker set, scheduler
idle, and absence of `EXCEPTION observed`.

This is a fixed mock-model/native fixture. It does not claim a remote model,
persistent disk, production Registry, or real hardware. Ring 0 only transports
bounded IPC words and enforces the existing capability/token syscalls; it does
not parse the semantic markers.

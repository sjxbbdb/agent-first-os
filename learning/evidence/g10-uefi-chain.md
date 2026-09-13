# G10 native UEFI/OVMF composite fixture

`product/tests/g10-uefi-chain-test.sh` builds the same `AGENT_OS_TEST_G10_NATIVE`
Ring 3 composite used by the BIOS test, then boots the same ELF kernel through
the self-written UEFI loader and OVMF. The serial log proves the loader type,
context/model/registry markers, one-shot policy token, service postcondition,
replay rejection, fixed postcondition failure with in-memory rollback, and
scheduler idle.

This is still a bounded mock-model/native fixture. It does not claim remote
model access, persistent journal storage, production Registry, or physical
hardware behavior.

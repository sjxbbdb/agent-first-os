# G9 native Agent Runtime service slice

`product/tests/g9-native-runtime-service-test.sh` builds the BIOS image with
`AGENT_OS_TEST_RUNTIME_SERVICE` and boots it in QEMU. The primary Ring 3
Runtime task sends a versioned `START` event, yields to a sibling Ring 3
Supervisor-side service, receives a `HEARTBEAT` acknowledgement, sends a
`CHECKPOINT` event, and yields so the service consumes the checkpoint. The
serial log proves all three IPC transfers and clean scheduler quiescence.

`product/tests/g9-uefi-runtime-service-test.sh` repeats the same user image
through the PE/COFF UEFI loader and OVMF; the loader type marker proves the
shared `kernel_entry` path. `g9-runtime-service-abi-test.sh` checks the fixed
width C ABI at compile time.

This is a bounded native transport/lifecycle slice. It does not prove a
general initrd service spawn, a persistent checkpoint store, a real pi or
remote model provider, capability issuance, Policy Firewall approval, or
hardware/production behavior. The kernel only transports the existing bounded
IPC message; Runtime policy and model code remain Ring 3.

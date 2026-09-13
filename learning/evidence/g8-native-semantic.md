# G8 native Ring 3 semantic slice

`g8-native-semantic-test.sh` boots the BIOS image in QEMU with two user
processes. The first is an Agent fixture and the second is a Policy/Registry
service. Both execute in Ring 3 and communicate only through the bounded IPC
endpoint.

The Agent submits an unknown tool identifier, a valid versioned tool lookup,
the current task-window identifier, an action digest, and a Trusted Input
nonce. The service denies the unknown tool and returns deterministic metadata
and digests for the accepted request. The Agent checks the reply opcode and
value before committing the result. A malformed or reordered reply reaches the
failure marker and cannot be treated as an approved action.

This is a native transport and service-contract slice, not a cryptographic
registry, real input device, or production policy implementation. The host
`policy_registry_runtime.js` remains the reference for canonical digests,
capability binding, expiry/replay, journal, and rollback semantics; the kernel
does not interpret these registry words.

Evidence command:

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g8-native-semantic-test.sh"
```

Expected marker:

```text
NATIVE REGISTRY CONTEXT TRUSTED INPUT OK
```

The same fixture is also run through the shared UEFI handoff with:

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g10-uefi-semantic-test.sh"
```

That test additionally requires `loader_type=2` from OVMF before accepting the
Ring 3 service marker.

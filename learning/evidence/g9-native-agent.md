# G9 native Agent/Policy IPC evidence

The `AGENT_OS_TEST_AGENT_LOOP` fixture has two ordinary Ring 3 processes. The
first behaves as an Agent Runtime boundary: it sends an action nonce over
`SYS_IPC_CALL`, yields, receives an opaque token, consumes it through the
kernel token syscall, and only then reports completion. The second behaves as
the Policy service: it receives the action, uses its bootstrap admin
capability to mint a token bound to the first process, and returns it over the
same endpoint. No model or policy framework is linked into Ring 0.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g9-native-agent-test.sh'
PASS: Ring 3 Agent sends an action, Policy mints a bound token, and Agent consumes it through IPC
```

The QEMU log contains IPC send/receive, policy token mint, token consume, and
`AGENT RING3 ACTION COMMITTED` before scheduler idle. This is a native ABI
vertical slice, not the complete G9 runtime: there is no pi/remote model
adapter, Semantic Registry process, native file/window service, postcondition
verifier, checkpoint/recovery, or Supervisor heartbeat integration yet.

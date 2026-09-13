# G10 UEFI Agent/Policy vertical evidence

This fixture runs the same native Agent/Policy IPC loop as the BIOS test
through the self-written UEFI loader. The Agent sends an action nonce, the
Ring 3 Policy service mints a capability-bound kernel token, and the Agent
consumes it before reporting completion.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g10-uefi-agent-test.sh'
PASS: OVMF shared kernel_entry runs the Ring 3 Agent/Policy token loop to completion
```

The log proves `loader_type=2`, policy token mint/consume, Agent completion,
and scheduler idle. This is only a deterministic native fixture: it does not
yet connect a remote/offline model adapter, Semantic Registry process, real
file/network/window service, or the full fault-injection matrix required to
close G10.

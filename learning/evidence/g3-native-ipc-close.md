# G3 native IPC endpoint close slice audit

## Attempt and blocker

The existing endpoint already has a host-side `agent_os_ipc_endpoint_close`
operation and capability lookup binds IPC use to the kernel endpoint object.
I added a provisional `SYS_IPC_CLOSE` path and a Ring 3 fixture, but the
targeted BIOS boot does not reach Ring 3 after the change.  The serial log
stops after the high-half contract and reports:

```text
HIGH HALF alias contract READY low-exec preserved
EXCEPTION observed
vector=0x000000000000000e
error=0x0000000000000010
EXCEPTION HALT
```

Therefore there is no green native endpoint lifecycle evidence and this must
not be counted as a completed G3 slice.  The host endpoint close behavior is
not sufficient evidence for kernel completion.

## Required follow-up

Localize the pre-Ring-3 page fault and verify process/kernel layout before
retaining the new syscall.  Then prove, in QEMU, that a capability with
`CAP_RIGHT_REVOKE` closes the endpoint and that a stale sender handle receives
the closed/error result.  Freeze/resume and timeout remain outside this slice.

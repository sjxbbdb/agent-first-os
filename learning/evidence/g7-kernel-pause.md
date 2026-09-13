# G7 native emergency pause and expiry evidence

Command:

```text
bash product/tests/g7-kernel-pause-test.sh
```

The BIOS/QEMU fixture runs the Policy service in Ring 3.  It mints a
generation-tagged action token, requests `SYS_POLICY_PAUSE` with the
kernel-owned policy-admin capability, and attempts to consume the token while
the gate is paused.  Ring 0 rejects that consume without advancing the token
clock.  `SYS_POLICY_RESUME` then reopens the gate and the same token can be
consumed exactly once.  A second token with logical expiry tick 1 is consumed
after the clock has advanced and is rejected with the timeout marker.

Observed markers:

```text
SYSCALL policy token mint OK
SYSCALL policy emergency pause OK
SYSCALL policy token consume paused
SYSCALL policy emergency resume OK
SYSCALL policy token consume OK
SYSCALL policy token consume timeout
SCHEDULER idle - all tasks exited
```

This proves a native emergency gate and expiry check, not a complete G7
transaction journal, snapshot, rollback, Trusted Input device, or production
Policy Firewall.

# G7 kernel token evidence

`policy_token.c` is the first native G7 security primitive. A policy authority
capability (Ring 3 slot owned by the policy fixture) mints a generation-tagged
token bound to the owner process, an opaque capability handle and an action
nonce. Consumption checks every binding and atomically invalidates the token;
the same token cannot be replayed. The expiry field and owner-only revoke path
are implemented in the primitive as well.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g7-kernel-token-test.sh'
PASS: Ring 3 policy authority rejects a forged nonce, permits the bound consume, and rejects replay
```

The QEMU log contains `SYSCALL policy token mint OK`, a forged action-nonce
consume denial, one valid consume success, and a second consume denial from
replay before both Ring 3 tasks reach scheduler idle. The forged attempt does
not consume the token, which is demonstrated by the following valid consume.
This is not yet the full Policy Firewall: token issuance is a bootstrap
fixture, the action digest is an explicit nonce rather than a kernel hash, and
journal, snapshot/rollback, Trusted Input and native Registry/Agent services
remain open.

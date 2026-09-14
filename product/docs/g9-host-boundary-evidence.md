# G9/G10 host boundary matrix

`product/tests/g9-host-boundary-test.sh` exercises the existing dependency-free
`PiAgentLoop` reference through four independent host-side cases:

- malformed `ActionPlan` is rejected before the executor;
- prompt-injection text cannot escape the policy-authorized task window;
- `OfflineNullAdapter` completes with a protocol-safe empty plan without a
  transport call;
- a failed postcondition is journaled, checkpointed, and recovered as the same
  failed task without requesting another model turn.

The test is intentionally host-only evidence for the Ring 3 reference boundary.
It does not prove a native service, kernel IPC, persistent filesystem semantics,
real pi/remote model behavior, or production readiness.

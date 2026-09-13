# G9 pi-shaped Ring 3 runtime evidence

The reference loop in `product/agent-runtime/reference/pi_runtime.js` is a
dependency-free Ring 3 state machine. It borrows the useful shape of pi's
session loop without importing pi, an OpenAI harness, or a model SDK:

- structured `agent.plan` responses are validated before any executor call;
- policy approval is explicit and one-time, with opaque tokens consumed by the
  policy boundary;
- append-only events have sequence numbers and a bounded pending queue;
- event backpressure pauses the producer until the consumer acknowledges a
  watermark;
- checkpoints include the plan cursor, response id, journal, approval state,
  events, and heartbeat timestamp, allowing a second Ring 3 loop instance to
  recover without asking the model for a duplicate turn;
- prepared actions are journaled before execution and committed only after the
  executor returns; executor failures enter `rollback_pending` semantics at
  the host policy/executor boundary;
- `monitor()` fails closed on a stale heartbeat.

Run the deterministic host evidence test:

```powershell
node product/tests/g9-pi-loop.test.js
```

Expected output:

```text
G9 pi-shaped Ring3 loop: PASS
evidence: executed=2 recovered=1 checkpoints=1
boundary: dependency-free Ring3 reference; no pi package, remote model, hardware, or kernel claim
```

The test covers structured response validation, malformed-plan rejection,
approval and recovery, duplicate model-turn avoidance, event backpressure and
acknowledgement, heartbeat timeout, and legacy-plan compatibility. It does not
claim a real pi installation, remote provider availability, native hardware
input, or persistence beyond the injected checkpoint store.

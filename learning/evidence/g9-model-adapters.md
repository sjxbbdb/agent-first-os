# G9 model adapter boundary evidence

`product/agent-runtime/reference/adapters.js` freezes the Ring 3 model boundary
without placing a model framework in the kernel. It provides an offline/null
adapter, a remote request adapter with plan validation, a pi-primary proposal
adapter, and a Codex harness/app-server-shaped adapter. All return the same
validated ActionPlan contract consumed by `AgentEngine`.

Verification:

```text
node product/tests/g9-adapter-test.js
G9 model adapter boundaries: PASS
boundary: adapters are Ring 3 reference interfaces; no network or framework runtime claim
```

The test uses deterministic injected functions. It does not claim a live pi
installation, remote model availability, or production cloud parity.

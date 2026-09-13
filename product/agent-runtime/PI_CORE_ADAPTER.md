# Optional pi-agent-core adapter

`pi-core-adapter.mjs` is a Ring 3 / host-only bridge. It accepts an already configured pi `Agent`, listens for its lifecycle events, sends the project envelope through `Agent.prompt`, and validates the assistant's JSON response as the versioned `ActionPlan` before returning it.

The dependency is optional and is never imported by G0/G3 kernel code. `offline: true` keeps the runtime dependency-free and returns a protocol-safe empty plan. The adapter does not provide a model, tool registry, capability, policy, approval, persistence, or a generic ELF/kernel path; those remain project boundaries.

The targeted smoke uses `@earendil-works/pi-agent-core@0.85.1` and its matching `@earendil-works/pi-ai` package from a temporary directory under `build/`. Do not commit `node_modules` or treat this host smoke as BIOS/QEMU evidence.

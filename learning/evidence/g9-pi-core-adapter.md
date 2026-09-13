# G9 pi Core adapter evidence

## Scope

This slice adds an optional host-only bridge from a configured `@earendil-works/pi-agent-core` `Agent` to the project's version-1 `ActionPlan`. It preserves the dependency-free offline path and does not add pi to kernel or BIOS build inputs.

## Red/green evidence

The old runtime had only a pi-shaped reference loop and no adapter that consumed a real `Agent` event stream. The new targeted test covers both paths:

```text
node product/tests/g9-pi-core-adapter.test.mjs
G9 Pi adapter offline OK

$env:PI_CORE_SMOKE='1'; node product/tests/g9-pi-core-adapter.test.mjs
G9 Pi adapter offline OK
G9 Pi adapter Agent/event stream OK
```

The real smoke used a temporary install of `@earendil-works/pi-agent-core@0.85.1` (MIT) under `build/pi-adapter-smoke`; its custom `streamFn` returned the package's `AssistantMessageEventStream`, and the adapter observed `agent_end` and validated the returned plan. `node_modules` is not a project artifact.

## Limits

This is host/API compatibility evidence only. It does not prove a network provider, model quality, tool execution, policy approval, persistence/recovery, BIOS/QEMU behavior, or kernel integration. The adapter requires the caller to configure those concerns and treats malformed or wrong-task plans as errors.

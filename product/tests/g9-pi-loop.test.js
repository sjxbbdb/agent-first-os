"use strict";

const assert = require("node:assert/strict");
const {
  FixturePolicy,
} = require("../agent-runtime/reference/runtime");
const {
  PiAgentLoop,
  MemoryCheckpointStore,
  MODEL_RESPONSE_TYPE,
  normalizeModelResponse,
} = require("../agent-runtime/reference/pi_runtime");

function response(taskId, responseId, actions, turn = 0) {
  return {
    version: 1,
    type: MODEL_RESPONSE_TYPE,
    task_id: taskId,
    response_id: responseId,
    turn,
    finish_reason: "tool_calls",
    plan: { version: 1, task_id: taskId, actions },
  };
}

function action(actionId, toolId, risk = "L1", params = {}) {
  return {
    action_id: actionId,
    tool_id: toolId,
    tool_version: "1",
    params,
    risk,
    idempotency_key: "model-value-is-replaced",
  };
}

async function main() {
  let now = 1_700_000_000_000;
  const store = new MemoryCheckpointStore();
  const executed = [];
  const plan = response("pi.demo", "turn-1", [
    action("write", "fs.write", "L1", { path: "notes.txt", content: "hello" }),
    action("publish", "net.publish", "L2", { target: "example" }),
  ]);
  const model = {
    calls: 0,
    async respond(envelope) {
      this.calls += 1;
      assert.equal(envelope.type, "task.context");
      return plan;
    },
  };
  const policy = new FixturePolicy({ toolRisks: { "fs.write": "L1", "net.publish": "L2" } });
  const executor = { execute: async (item) => { executed.push(item.action_id); return { ok: true, action_id: item.action_id }; } };
  const loop = new PiAgentLoop({ model, policy, executor, checkpointStore: store, nowMs: () => now, maxOutstandingEvents: 128 });

  let snapshot = await loop.start({ task_id: "pi.demo", context: { current_window: "editor" }, risk_budget: "L2" });
  assert.equal(snapshot.state, "awaiting_approval");
  assert.deepEqual(executed, ["write"]);
  assert.equal(snapshot.response_id, "turn-1");
  assert.ok(snapshot.events.some((event) => event.type === "model.response"));
  assert.ok(store.load("pi.demo"), "approval boundary must be checkpointed");

  /* A second Ring 3 runtime can recover the approval boundary and continue. */
  const recoveredLoop = new PiAgentLoop({ model, policy, executor, checkpointStore: store, nowMs: () => now, maxOutstandingEvents: 128 });
  snapshot = recoveredLoop.recover("pi.demo");
  assert.equal(snapshot.state, "awaiting_approval");
  snapshot = await recoveredLoop.approve("pi.demo", { token: snapshot.confirmation.token });
  assert.equal(snapshot.state, "completed");
  assert.deepEqual(executed, ["write", "publish"]);
  assert.equal(model.calls, 1, "recovery must not ask the model for a duplicate turn");
  assert.equal(await policy.consumeConfirmation("fixture:missing", plan.plan.actions[1], { task_id: "pi.demo" }), false);

  /* Structured malformed output fails before any executor call. */
  let malformedExecuted = false;
  const malformed = new PiAgentLoop({
    model: { respond: async () => ({ version: 1, type: MODEL_RESPONSE_TYPE, task_id: "bad", response_id: "bad", plan: { version: 1, task_id: "bad", actions: [{ action_id: "x", tool_id: "fs.write", tool_version: "1", params: {}, risk: "L0", idempotency_key: "x", unexpected: true }] } }) },
    policy,
    executor: { execute: async () => { malformedExecuted = true; return { ok: true }; } },
    maxOutstandingEvents: 128,
  });
  snapshot = await malformed.start({ task_id: "bad" });
  assert.equal(snapshot.state, "failed");
  assert.equal(malformedExecuted, false);
  assert.match(snapshot.events.at(-1).payload.code, /invalid_model_response/);

  /* Event backpressure pauses the producer until the consumer acknowledges. */
  const backpressure = new PiAgentLoop({
    model: { respond: async (envelope) => response(envelope.task_id, "empty-1", [], 0) },
    policy,
    executor,
    maxOutstandingEvents: 2,
    nowMs: () => now,
  });
  snapshot = await backpressure.start({ task_id: "pressure" });
  assert.equal(snapshot.state, "paused");
  assert.equal(snapshot.pause_reason, "backpressure");
  const watermark = snapshot.events.at(-1).sequence;
  snapshot = backpressure.ackThrough("pressure", watermark);
  assert.equal(snapshot.state, "running");
  snapshot = await backpressure.step("pressure");
  assert.equal(snapshot.state, "paused");
  snapshot = backpressure.ackThrough("pressure", snapshot.events.at(-1).sequence);
  snapshot = await backpressure.step("pressure");
  assert.equal(snapshot.state, "completed");

  /* Supervisor-style heartbeat timeout is fail-closed. */
  const heartbeat = new PiAgentLoop({
    model: { respond: async (envelope) => response(envelope.task_id, "never-reached", [], 0) },
    policy,
    executor,
    maxOutstandingEvents: 2,
    heartbeatTimeoutMs: 10,
    nowMs: () => now,
  });
  snapshot = await heartbeat.start({ task_id: "heartbeat" });
  assert.equal(snapshot.state, "paused");
  now += 11;
  snapshot = heartbeat.monitor("heartbeat");
  assert.equal(snapshot.state, "failed");
  assert.equal(snapshot.events.at(-1).payload.code, "heartbeat_timeout");

  const legacy = normalizeModelResponse({ version: 1, task_id: "legacy", actions: [] }, "legacy", 0);
  assert.equal(legacy.ok, true);
  assert.match(legacy.response.response_id, /^legacy:/);
  console.log("G9 pi-shaped Ring3 loop: PASS");
  console.log(`evidence: executed=${executed.length} recovered=${snapshot.events.filter((event) => event.type === "task.failed").length} checkpoints=${store.records.size}`);
  console.log("boundary: dependency-free Ring3 reference; no pi package, remote model, hardware, or kernel claim");
}

main().catch((error) => { console.error(error); process.exitCode = 1; });

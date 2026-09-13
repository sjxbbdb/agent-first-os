"use strict";

const assert = require("node:assert/strict");
const {
  AgentEngine,
  FixturePolicy,
  MockModelAdapter,
  deriveIdempotencyKey,
  normalizeActionPlan,
  validateActionPlan,
} = require("./runtime");

async function main() {
  const sourcePlan = {
    version: 1,
    task_id: "demo.task",
    actions: [
      { action_id: "write", tool_id: "fs.write", tool_version: "1", params: { path: "notes.txt", content: "hello" }, risk: "L1", idempotency_key: "model-value" },
      { action_id: "publish", tool_id: "net.publish", tool_version: "1", params: { target: "example" }, risk: "L2", idempotency_key: "model-value" },
    ],
  };
  const normalized = normalizeActionPlan(sourcePlan);
  assert.equal(validateActionPlan(normalized).ok, true);
  assert.equal(normalized.actions[0].idempotency_key, deriveIdempotencyKey("demo.task", sourcePlan.actions[0]));
  assert.equal(validateActionPlan({ ...normalized, actions: [{ ...normalized.actions[0], action_id: "same" }, { ...normalized.actions[1], action_id: "same" }] }).ok, false);

  const executed = [];
  const model = new MockModelAdapter({ "demo.task": sourcePlan });
  const engine = new AgentEngine({ model, policy: new FixturePolicy({ toolRisks: { "fs.write": "L1", "net.publish": "L2" } }), executor: { execute: async (action) => { executed.push(action.tool_id); return { ok: true, echoed: action.params }; } } });
  let snapshot = await engine.start({ task_id: "demo.task", context: { window: "current" }, risk_budget: "L2" });
  assert.equal(snapshot.state, "awaiting_confirmation");
  assert.deepEqual(executed, ["fs.write"]);
  snapshot = await engine.resume("demo.task", { token: snapshot.confirmation.token });
  assert.equal(snapshot.state, "completed");
  assert.deepEqual(executed, ["fs.write", "net.publish"]);
  assert.equal(model.calls.length, 1);
  const events = engine.jsonl("demo.task").trim().split("\n").map(JSON.parse);
  assert.ok(events.length >= 6);
  assert.deepEqual(events.map((event) => event.sequence), events.map((_, index) => index));

  const denied = new AgentEngine({ model: new MockModelAdapter({ blocked: { version: 1, task_id: "blocked", actions: [{ action_id: "x", tool_id: "shell.exec", tool_version: "1", params: {}, risk: "L0", idempotency_key: "x" }] } }), policy: { check: async () => ({ decision: "deny", risk: "L0", reason: "test" }) }, executor: { execute: async () => assert.fail("denied action executed") } });
  assert.equal((await denied.start({ task_id: "blocked" })).state, "failed");

  assert.equal(validateActionPlan({ version: 1, task_id: "bad", actions: {} }).ok, false);
  assert.equal(validateActionPlan({ version: 1, task_id: "bad", actions: [null] }).ok, false);
  const malformed = new AgentEngine({ model: new MockModelAdapter({ bad: { version: 1, task_id: "bad", actions: [null] } }), policy: new FixturePolicy(), executor: { execute: async () => assert.fail("invalid action executed") } });
  assert.equal((await malformed.start({ task_id: "bad" })).state, "failed");
  const spoofedRisk = await new FixturePolicy({ toolRisks: { "net.publish": "L3" } }).check({ tool_id: "net.publish", risk: "L0" });
  assert.equal(spoofedRisk.decision, "confirm");
  assert.equal(spoofedRisk.risk, "L3");
  console.log("agent-runtime conformance: PASS");
}

main().catch((error) => { console.error(error); process.exitCode = 1; });

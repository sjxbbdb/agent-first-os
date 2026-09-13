"use strict";

const assert = require("node:assert/strict");
const {
  OfflineNullAdapter,
  RemoteModelAdapter,
  PiAgentAdapter,
  CodexHarnessAdapter,
} = require("../agent-runtime/reference/adapters");

const envelope = { version: 1, type: "task.context", task_id: "adapter.task", sequence: 0, payload: {} };
const plan = { version: 1, task_id: "adapter.task", actions: [] };

async function main() {
  const offline = new OfflineNullAdapter();
  assert.deepEqual(await offline.propose(envelope), plan);
  assert.equal(offline.calls, 1);

  let seen;
  const remote = new RemoteModelAdapter({ request: async (value) => { seen = value; return plan; } });
  assert.deepEqual(await remote.propose(envelope), plan);
  assert.equal(seen.task_id, "adapter.task");
  const offlineRemote = new RemoteModelAdapter({ request: async () => { throw new Error("network"); }, offline: true });
  assert.deepEqual(await offlineRemote.propose(envelope), plan);
  const networkFallback = new OfflineNullAdapter();
  const disconnected = new RemoteModelAdapter({ request: async () => { throw new Error("network"); }, fallback: networkFallback });
  assert.deepEqual(await disconnected.propose(envelope), plan);
  assert.equal(networkFallback.calls, 1);

  const pi = new PiAgentAdapter({ propose: async () => plan });
  assert.deepEqual(await pi.propose(envelope), plan);
  let method;
  const codex = new CodexHarnessAdapter({ request: async (name, value) => { method = [name, value.task_id]; return plan; } });
  assert.deepEqual(await codex.propose(envelope), plan);
  assert.deepEqual(method, ["agent.plan", "adapter.task"]);

  await assert.rejects(() => remote.propose({ ...envelope, task_id: "" }), /invalid task.context/);
  await assert.rejects(() => new RemoteModelAdapter({ request: async () => ({ version: 1, task_id: "other", actions: [] }) }).propose(envelope), /invalid plan/);
  console.log("G9 model adapter boundaries: PASS");
  console.log("boundary: adapters are Ring 3 reference interfaces; no network or framework runtime claim");
}

main().catch((error) => { console.error(error); process.exitCode = 1; });

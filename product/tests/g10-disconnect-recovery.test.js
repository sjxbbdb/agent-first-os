"use strict";

const assert = require("node:assert/strict");
const { HttpRemoteModelAdapter } = require("../agent-runtime/reference/adapters");

async function main() {
  let calls = 0;
  const adapter = new HttpRemoteModelAdapter({
    url: "https://model.invalid/plan",
    maxAttempts: 2,
    request: undefined,
    fetchImpl: async () => {
      calls += 1;
      if (calls === 1) throw new Error("socket disconnected");
      return {
        ok: true,
        async json() {
          return { version: 1, task_id: "disconnect.task", actions: [] };
        },
      };
    },
  });
  const plan = await adapter.propose({
    version: 1,
    type: "task.context",
    task_id: "disconnect.task",
    sequence: 0,
    payload: {},
  });
  assert.equal(calls, 2, "one bounded retry should follow a transient disconnect");
  assert.deepEqual(plan, { version: 1, task_id: "disconnect.task", actions: [] });
  console.log("G10 disconnect recovery: PASS (bounded retry returns validated plan)");
  console.log("boundary: host-only HTTP adapter; no native/QEMU remote model claim");
}

main().catch((error) => { console.error(error); process.exitCode = 1; });

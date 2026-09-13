"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { DurableCheckpointStore } = require("../agent-runtime/reference/pi_runtime");

function main() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "agent-os-g9-checkpoint-"));
  const file = path.join(dir, "checkpoint.jsonl");
  try {
    let store = new DurableCheckpointStore({ file });
    store.save("task-1", { state: "running", cursor: 1 });
    store.save("task-1", { state: "awaiting_approval", cursor: 1, token: "opaque" });
    store.save("task-2", { state: "completed", cursor: 2 });
    assert.deepEqual(store.load("task-1"), { state: "awaiting_approval", cursor: 1, token: "opaque" });
    store.close();

    store = new DurableCheckpointStore({ file });
    assert.deepEqual(store.load("task-1"), { state: "awaiting_approval", cursor: 1, token: "opaque" });
    assert.deepEqual(store.load("task-2"), { state: "completed", cursor: 2 });
    assert.equal(store.remove("task-2"), true);
    store.close();
    store = new DurableCheckpointStore({ file });
    assert.equal(store.load("task-2"), null);
    store.close();

    fs.appendFileSync(file, JSON.stringify({ sequence: 99, task_id: "tail", snapshot: {} }).slice(0, -3));
    assert.throws(() => new DurableCheckpointStore({ file }), /truncated|invalid JSON/);
    fs.writeFileSync(file, `${JSON.stringify({ sequence: 0, task_id: "bad", snapshot: {} })}\nnot-json\n`);
    assert.throws(() => new DurableCheckpointStore({ file }), /invalid JSON/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  console.log("G9 durable checkpoint store: PASS");
  console.log("boundary: host-side fsync/reopen checkpoint reference; no native crash-consistent filesystem claim");
}

main();

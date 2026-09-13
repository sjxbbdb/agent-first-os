"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { DurableJSONLJournal } = require("../services/policy_registry_runtime");

function main() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "agent-os-g7-journal-"));
  const file = path.join(dir, "policy.jsonl");
  try {
    let journal = new DurableJSONLJournal({ file });
    journal.append({ phase: "prepare", action_id: "a1", task_id: "t1" }, {
      snapshot: { kind: "file", path: "notes.txt", digest: "sha256:before" },
      preimage: { kind: "bytes", size: 5 },
    });
    journal.append({ phase: "commit", action_id: "a1", task_id: "t1" });
    journal.append({ phase: "prepare", action_id: "a2", task_id: "t1" });
    journal.append({ phase: "rollback_pending", action_id: "a2", task_id: "t1", external_effect: "unknown" });
    assert.equal(fs.readFileSync(file, "utf8").endsWith("\n"), true);
    journal = new DurableJSONLJournal({ file });
    assert.deepEqual(journal.recover(), [
      { action_id: "a1", state: "committed" },
      { action_id: "a2", state: "rollback_pending" },
    ]);
    assert.deepEqual(journal.getSnapshotMetadata("a1"), { kind: "file", path: "notes.txt", digest: "sha256:before" });
    assert.deepEqual(journal.getPreimageMetadata("a1"), { kind: "bytes", size: 5 });
    assert.equal(journal.records[3].sequence, 3);

    fs.appendFileSync(file, JSON.stringify({ sequence: 4, phase: "commit", action_id: "a3" }).slice(0, -2));
    assert.throws(() => new DurableJSONLJournal({ file }), /journal tail is truncated|invalid JSON/);
    fs.writeFileSync(file, `${JSON.stringify({ sequence: 0, phase: "prepare", action_id: "bad" })}\nnot-json\n`);
    assert.throws(() => new DurableJSONLJournal({ file }), /invalid JSON/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  console.log("G7 durable JSONL journal: PASS");
  console.log("boundary: host-side append/fsync and recovery metadata only; no real disk transaction claim");
}

main();

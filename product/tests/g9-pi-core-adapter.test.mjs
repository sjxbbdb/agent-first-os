import assert from "node:assert/strict";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { createPiCoreAdapter } from "../agent-runtime/pi-core-adapter.mjs";

const envelope = { version: 1, task_id: "g9-smoke", sequence: 1, input: {} };
const offline = createPiCoreAdapter({ offline: true });
assert.deepEqual(await offline.propose(envelope), { version: 1, task_id: "g9-smoke", actions: [] });
console.log("G9 Pi adapter offline OK");

if (process.env.PI_CORE_SMOKE === "1") {
  const root = process.env.PI_CORE_ROOT ?? path.resolve("build/pi-adapter-smoke/node_modules/@earendil-works/pi-agent-core");
  const { Agent } = await import(pathToFileURL(path.join(root, "dist/index.js")));
  const { AssistantMessageEventStream } = await import(pathToFileURL(path.join(path.dirname(root), "pi-ai/dist/index.js")));
  const plan = JSON.stringify({ version: 1, task_id: envelope.task_id, actions: [] });
  const agent = new Agent({
    streamFn: () => {
      const stream = new AssistantMessageEventStream();
      const message = { role: "assistant", content: [{ type: "text", text: plan }], api: "smoke", provider: "smoke", model: "smoke", usage: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, totalTokens: 0, cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0, total: 0 } }, stopReason: "stop", timestamp: Date.now() };
      stream.push({ type: "start", partial: message });
      stream.push({ type: "done", reason: "stop", message });
      return stream;
    },
  });
  const events = [];
  const adapter = createPiCoreAdapter({ agent, onEvent: (event) => events.push(event.type) });
  assert.deepEqual(await adapter.propose(envelope), { version: 1, task_id: envelope.task_id, actions: [] });
  assert.ok(events.includes("agent_end"));
  console.log("G9 Pi adapter Agent/event stream OK");
}

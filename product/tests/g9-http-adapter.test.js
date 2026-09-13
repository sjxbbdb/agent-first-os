"use strict";

const assert = require("node:assert/strict");
const http = require("node:http");
const { HttpRemoteModelAdapter, OfflineNullAdapter } = require("../agent-runtime/reference/adapters");

const envelope = {
  version: 1,
  type: "task.context",
  task_id: "http.demo",
  sequence: 0,
  turn: 0,
  payload: { context: { current_window: "editor" }, risk_budget: "L1" },
};

const plan = { version: 1, task_id: "http.demo", actions: [] };

async function main() {
  const requests = [];
  const adapter = new HttpRemoteModelAdapter({
    url: "https://model.example/v1/agent",
    headers: { authorization: "Bearer test-only" },
    fetchImpl: async (url, options) => {
      requests.push({ url, options });
      return { ok: true, status: 200, json: async () => plan };
    },
  });
  assert.deepEqual(await adapter.propose(envelope), plan);
  assert.equal(requests[0].url, "https://model.example/v1/agent");
  assert.equal(requests[0].options.method, "POST");
  assert.equal(requests[0].options.headers.authorization, "Bearer test-only");
  assert.deepEqual(JSON.parse(requests[0].options.body), envelope);

  const fallback = new OfflineNullAdapter();
  const failed = new HttpRemoteModelAdapter({
    url: "https://model.example/v1/agent",
    fetchImpl: async () => ({ ok: false, status: 503, json: async () => ({}) }),
    fallback,
  });
  assert.deepEqual(await failed.propose(envelope), { version: 1, task_id: "http.demo", actions: [] });

  const malformed = new HttpRemoteModelAdapter({
    url: "https://model.example/v1/agent",
    fetchImpl: async () => ({ ok: true, status: 200, json: async () => ({ version: 1, task_id: "wrong", actions: [] }) }),
  });
  await assert.rejects(() => malformed.propose(envelope), /invalid plan/);

  /* Exercise the concrete Node fetch path against a loopback HTTP endpoint. */
  const server = http.createServer((request, response) => {
    let body = "";
    request.setEncoding("utf8");
    request.on("data", (chunk) => { body += chunk; });
    request.on("end", () => {
      assert.deepEqual(JSON.parse(body), envelope);
      response.writeHead(200, { "content-type": "application/json" });
      response.end(JSON.stringify(plan));
    });
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  try {
    const address = server.address();
    const live = new HttpRemoteModelAdapter({ url: `http://127.0.0.1:${address.port}/agent`, timeoutMs: 2_000 });
    assert.deepEqual(await live.propose(envelope), plan);
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
  console.log("G9 HTTP remote adapter: PASS");
  console.log("boundary: real HTTP-shaped Ring 3 transport with injected fetch; no provider, credential, or production model claim");
}

main().catch((error) => { console.error(error); process.exitCode = 1; });

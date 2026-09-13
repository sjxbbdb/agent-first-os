"use strict";

const { PROTOCOL_VERSION, validateActionPlan } = require("./runtime");

function emptyPlan(taskId) {
  return { version: PROTOCOL_VERSION, task_id: taskId, actions: [] };
}

function checkEnvelope(envelope) {
  if (!envelope || envelope.version !== PROTOCOL_VERSION ||
      typeof envelope.task_id !== "string" || envelope.task_id.length === 0) {
    throw new TypeError("invalid task.context envelope");
  }
}

class OfflineNullAdapter {
  constructor() { this.calls = 0; }

  async propose(envelope) {
    checkEnvelope(envelope);
    this.calls += 1;
    return emptyPlan(envelope.task_id);
  }
}

class RemoteModelAdapter {
  constructor({ request, offline = false, fallback = null } = {}) {
    if (typeof request !== "function") throw new TypeError("request is required");
    this.request = request;
    this.offline = offline;
    if (fallback !== null && typeof fallback.propose !== "function") throw new TypeError("fallback.propose is required");
    this.fallback = fallback;
  }

  async propose(envelope) {
    checkEnvelope(envelope);
    if (this.offline) return this.fallback ? this.fallback.propose(envelope) : emptyPlan(envelope.task_id);
    let plan;
    try {
      plan = await this.request(envelope);
    } catch (error) {
      if (this.fallback) return this.fallback.propose(envelope);
      throw error;
    }
    const validation = validateActionPlan(plan);
    if (!validation.ok || plan.task_id !== envelope.task_id) {
      throw new Error(`remote model returned invalid plan: ${validation.errors.join("; ")}`);
    }
    return plan;
  }
}

/* A concrete JSON-over-HTTP transport for Ring 3.  The endpoint is kept
 * provider-neutral: it receives a task.context envelope and must return a
 * versioned ActionPlan (or an agent.plan envelope containing one). */
class HttpRemoteModelAdapter {
  constructor({ url, fetchImpl = globalThis.fetch, headers = {}, timeoutMs = 30_000, maxAttempts = 1, offline = false, fallback = null } = {}) {
    if (typeof url !== "string" || !/^https?:\/\//.test(url)) throw new TypeError("url must be an http(s) URL");
    if (typeof fetchImpl !== "function") throw new TypeError("fetchImpl is required");
    if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) throw new TypeError("timeoutMs must be positive");
    if (!Number.isInteger(maxAttempts) || maxAttempts < 1 || maxAttempts > 3) throw new TypeError("maxAttempts must be an integer from 1 to 3");
    if (fallback !== null && typeof fallback.propose !== "function") throw new TypeError("fallback.propose is required");
    this.url = url;
    this.fetchImpl = fetchImpl;
    this.headers = { "content-type": "application/json", ...headers };
    this.timeoutMs = timeoutMs;
    this.maxAttempts = maxAttempts;
    this.offline = offline;
    this.fallback = fallback;
  }

  async propose(envelope) {
    checkEnvelope(envelope);
    if (this.offline) return this.fallback ? this.fallback.propose(envelope) : emptyPlan(envelope.task_id);
    let response;
    let lastError;
    for (let attempt = 0; attempt < this.maxAttempts; attempt += 1) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), this.timeoutMs);
      try {
        response = await this.fetchImpl(this.url, {
          method: "POST",
          headers: this.headers,
          body: JSON.stringify(envelope),
          signal: controller.signal,
        });
        break;
      } catch (error) {
        lastError = error;
      } finally {
        clearTimeout(timer);
      }
    }
    if (!response) {
      if (this.fallback) return this.fallback.propose(envelope);
      throw new Error(`remote model request failed: ${String(lastError && (lastError.message || lastError))}`);
    }
    if (!response || response.ok !== true) {
      if (this.fallback) return this.fallback.propose(envelope);
      throw new Error(`remote model HTTP failure: ${response && response.status !== undefined ? response.status : "invalid response"}`);
    }
    let raw;
    try {
      raw = await response.json();
    } catch (error) {
      if (this.fallback) return this.fallback.propose(envelope);
      throw new Error(`remote model returned invalid JSON: ${String(error && (error.message || error))}`);
    }
    const plan = raw && raw.type === "agent.plan" && raw.plan ? raw.plan : raw;
    const validation = validateActionPlan(plan);
    if (!validation.ok || plan.task_id !== envelope.task_id) {
      throw new Error(`remote model returned invalid plan: ${validation.errors.join("; ")}`);
    }
    return plan;
  }
}

/* pi is the primary integration target. The adapter consumes a narrow
 * proposal function so pi session/event-stream code stays in Ring 3. */
class PiAgentAdapter {
  constructor({ propose } = {}) {
    if (typeof propose !== "function") throw new TypeError("pi propose is required");
    this.proposeFn = propose;
  }

  async propose(envelope) {
    checkEnvelope(envelope);
    const plan = await this.proposeFn(envelope);
    const validation = validateActionPlan(plan);
    if (!validation.ok || plan.task_id !== envelope.task_id) {
      throw new Error(`pi returned invalid plan: ${validation.errors.join("; ")}`);
    }
    return plan;
  }
}

/* Codex app-server/harness can use the same proposal contract without being
 * linked into the kernel or dictating the AgentEngine event model. */
class CodexHarnessAdapter {
  constructor({ request } = {}) {
    if (typeof request !== "function") throw new TypeError("Codex request is required");
    this.request = request;
  }

  async propose(envelope) {
    checkEnvelope(envelope);
    const plan = await this.request("agent.plan", envelope);
    const validation = validateActionPlan(plan);
    if (!validation.ok || plan.task_id !== envelope.task_id) {
      throw new Error(`Codex returned invalid plan: ${validation.errors.join("; ")}`);
    }
    return plan;
  }
}

module.exports = { OfflineNullAdapter, RemoteModelAdapter, HttpRemoteModelAdapter, PiAgentAdapter, CodexHarnessAdapter };

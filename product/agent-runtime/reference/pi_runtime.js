"use strict";

const fs = require("node:fs");

/*
 * A small, dependency-free Ring 3 execution loop shaped after pi's useful
 * semantics: structured model turns, an append-only event stream, explicit
 * approval, bounded backpressure, heartbeat supervision and checkpoint
 * recovery.  This is a reference implementation of the boundary; it does
 * not import pi, Codex, or any model SDK and must never be linked into Ring 0.
 */

const {
  PROTOCOL_VERSION,
  canonicalJson,
  digest,
  normalizeActionPlan,
  validateActionPlan,
} = require("./runtime");

const MODEL_RESPONSE_TYPE = "agent.plan";
const ACTIVE_STATES = new Set(["created", "running", "awaiting_approval", "paused"]);

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function clone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

function defaultNow() {
  return Date.now();
}

function eventTime(nowMs) {
  return new Date(nowMs()).toISOString();
}

/**
 * Validate a structured model turn and return a runtime-owned ActionPlan.
 * Legacy bare ActionPlans are accepted only as a compatibility boundary and
 * receive a deterministic response id; new integrations should return the
 * explicit `{ type: "agent.plan", plan: ... }` envelope.
 */
function normalizeModelResponse(raw, taskId, turn) {
  if (!isRecord(raw)) return { ok: false, errors: ["model response must be an object"] };

  let response = raw;
  if (raw.type === undefined && Array.isArray(raw.actions)) {
    response = {
      version: PROTOCOL_VERSION,
      type: MODEL_RESPONSE_TYPE,
      task_id: raw.task_id,
      response_id: `legacy:${digest(raw)}`,
      turn,
      plan: raw,
    };
  }

  const errors = [];
  if (response.version !== PROTOCOL_VERSION) errors.push("response.version must be 1");
  if (response.type !== MODEL_RESPONSE_TYPE) errors.push(`response.type must be ${MODEL_RESPONSE_TYPE}`);
  if (response.task_id !== taskId) errors.push("response.task_id does not match task");
  if (typeof response.response_id !== "string" || response.response_id.length === 0) errors.push("response.response_id is required");
  if (response.turn !== undefined && (!Number.isInteger(response.turn) || response.turn < 0)) errors.push("response.turn must be a non-negative integer");
  if (!isRecord(response.plan)) errors.push("response.plan is required");

  const plan = response.plan;
  const planValidation = validateActionPlan(plan);
  if (!planValidation.ok) errors.push(...planValidation.errors.map((error) => `plan.${error}`));
  if (isRecord(plan) && plan.task_id !== taskId) errors.push("plan.task_id does not match task");

  if (errors.length > 0) return { ok: false, errors };
  return {
    ok: true,
    response: clone(response),
    plan: normalizeActionPlan(plan),
  };
}

/** A deliberately simple durable-looking store used by tests and embedding hosts. */
class MemoryCheckpointStore {
  constructor() {
    this.records = new Map();
  }

  save(taskId, snapshot) {
    this.records.set(taskId, clone(snapshot));
  }

  load(taskId) {
    return clone(this.records.get(taskId) || null);
  }

  remove(taskId) {
    return this.records.delete(taskId);
  }
}

/* Host-side durable checkpoint reference.  The native OS still needs a
 * crash-consistent filesystem service; this class only freezes the Ring 3
 * persistence boundary used by the Agent Runtime. */
class DurableCheckpointStore {
  constructor({ file } = {}) {
    if (typeof file !== "string" || file.length === 0) throw new TypeError("checkpoint file is required");
    this.file = file;
    this.records = new Map();
    this.sequence = 0;
    this._fd = fs.openSync(file, "a+");
    try {
      this._recover();
    } catch (error) {
      fs.closeSync(this._fd);
      this._fd = null;
      throw error;
    }
  }

  _recover() {
    const bytes = fs.readFileSync(this.file);
    if (bytes.length === 0) return;
    let text;
    try {
      text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    } catch (error) {
      throw new Error(`checkpoint contains invalid UTF-8: ${error.message}`);
    }
    if (!text.endsWith("\n")) throw new Error("checkpoint tail is truncated (missing newline)");
    for (const [index, line] of text.slice(0, -1).split("\n").entries()) {
      let record;
      try { record = JSON.parse(line); } catch (error) { throw new Error(`checkpoint contains invalid JSON at line ${index + 1}: ${error.message}`); }
      if (!isRecord(record) || !Number.isInteger(record.sequence) || record.sequence !== index || typeof record.task_id !== "string" || record.task_id.length === 0) {
        throw new Error(`checkpoint record ${index + 1} is invalid`);
      }
      if (record.deleted === true) this.records.delete(record.task_id);
      else if (isRecord(record.snapshot)) this.records.set(record.task_id, clone(record.snapshot));
      else throw new Error(`checkpoint record ${index + 1} has invalid snapshot`);
    }
    this.sequence = text.slice(0, -1).split("\n").length;
  }

  _append(record) {
    if (this._fd === null) throw new Error("checkpoint store is closed");
    const entry = { sequence: this.sequence, ...record };
    const bytes = Buffer.from(`${JSON.stringify(entry)}\n`, "utf8");
    let offset = 0;
    while (offset < bytes.length) offset += fs.writeSync(this._fd, bytes, offset, bytes.length - offset);
    fs.fsyncSync(this._fd);
    this.sequence += 1;
    return entry;
  }

  save(taskId, snapshot) {
    if (typeof taskId !== "string" || taskId.length === 0 || !isRecord(snapshot)) throw new TypeError("taskId and snapshot are required");
    this._append({ task_id: taskId, snapshot: clone(snapshot) });
    this.records.set(taskId, clone(snapshot));
  }

  load(taskId) {
    return clone(this.records.get(taskId) || null);
  }

  remove(taskId) {
    if (!this.records.has(taskId)) return false;
    this._append({ task_id: taskId, deleted: true });
    this.records.delete(taskId);
    return true;
  }

  close() {
    if (this._fd !== null) {
      fs.closeSync(this._fd);
      this._fd = null;
    }
  }
}

class PiAgentLoop {
  constructor({
    model,
    policy,
    executor,
    checkpointStore = new MemoryCheckpointStore(),
    nowMs = defaultNow,
    onEvent = () => {},
    maxOutstandingEvents = 64,
    heartbeatTimeoutMs = 30_000,
  } = {}) {
    if (!model || (typeof model.propose !== "function" && typeof model.respond !== "function")) throw new TypeError("model.propose or model.respond is required");
    if (!policy || typeof policy.check !== "function") throw new TypeError("policy.check is required");
    if (!executor || typeof executor.execute !== "function") throw new TypeError("executor.execute is required");
    if (!checkpointStore || typeof checkpointStore.save !== "function" || typeof checkpointStore.load !== "function") throw new TypeError("checkpointStore.save/load are required");
    if (!Number.isInteger(maxOutstandingEvents) || maxOutstandingEvents < 2) throw new TypeError("maxOutstandingEvents must be at least 2");
    if (!Number.isFinite(heartbeatTimeoutMs) || heartbeatTimeoutMs <= 0) throw new TypeError("heartbeatTimeoutMs must be positive");
    this.model = model;
    this.policy = policy;
    this.executor = executor;
    this.checkpointStore = checkpointStore;
    this.nowMs = nowMs;
    this.onEvent = onEvent;
    this.maxOutstandingEvents = maxOutstandingEvents;
    this.heartbeatTimeoutMs = heartbeatTimeoutMs;
    this.tasks = new Map();
  }

  _event(task, type, payload = {}) {
    const event = {
      version: PROTOCOL_VERSION,
      type,
      task_id: task.task_id,
      sequence: task.sequence++,
      at: eventTime(this.nowMs),
      payload: clone(payload),
    };
    task.events.push(event);
    task.pendingEvents.push(event);
    this.onEvent(event);
    if (task.pendingEvents.length >= this.maxOutstandingEvents && ACTIVE_STATES.has(task.state) && task.state !== "awaiting_approval") {
      task.state = "paused";
      task.pauseReason = "backpressure";
      /* The event above is the watermark. The consumer must acknowledge it. */
    }
    return event;
  }

  _task(taskId) {
    const task = this.tasks.get(taskId);
    if (!task) throw new Error(`task not found: ${taskId}`);
    return task;
  }

  _snapshot(task) {
    return {
      runtime: "pi-reference-v1",
      task_id: task.task_id,
      context: clone(task.context),
      risk_budget: task.risk_budget,
      state: task.state,
      pause_reason: task.pauseReason || null,
      sequence: task.sequence,
      turn: task.turn,
      cursor: task.cursor,
      plan: clone(task.plan),
      response_id: task.responseId,
      confirmation: clone(task.confirmation),
      approved_action_id: task.approvedActionId,
      journal: clone(task.journal),
      events: clone(task.events),
      pending_events: clone(task.pendingEvents),
      last_heartbeat_ms: task.lastHeartbeatMs,
    };
  }

  _save(task) {
    this.checkpointStore.save(task.task_id, this._snapshot(task));
  }

  _fail(task, code, details = {}) {
    task.state = "failed";
    task.pauseReason = null;
    this._event(task, "task.failed", { code, details });
    this._save(task);
    return this.snapshot(task.task_id);
  }

  _heartbeat(task, force = false) {
    if (!force && !ACTIVE_STATES.has(task.state)) return;
    task.lastHeartbeatMs = this.nowMs();
    this._event(task, "agent.heartbeat", {
      live: true,
      state: task.state,
      cursor: task.cursor,
      turn: task.turn,
    });
    this._save(task);
  }

  async start({ task_id, context = {}, risk_budget = "L1" } = {}) {
    if (typeof task_id !== "string" || task_id.length === 0) throw new TypeError("task_id is required");
    if (this.tasks.has(task_id)) throw new Error(`task already exists: ${task_id}`);
    const task = {
      task_id,
      context: clone(context),
      risk_budget,
      state: "created",
      pauseReason: null,
      sequence: 0,
      turn: 0,
      cursor: 0,
      plan: null,
      responseId: null,
      confirmation: null,
      approvedActionId: null,
      journal: [],
      events: [],
      pendingEvents: [],
      lastHeartbeatMs: this.nowMs(),
    };
    this.tasks.set(task_id, task);
    this._event(task, "task.open", { context: task.context, risk_budget });
    task.state = "running";
    this._event(task, "task.running", { loop: "pi-reference-v1" });
    this._save(task);
    return this.step(task_id);
  }

  async _requestModel(task) {
    const envelope = {
      version: PROTOCOL_VERSION,
      type: "task.context",
      task_id: task.task_id,
      sequence: task.sequence,
      turn: task.turn,
      payload: {
        context: clone(task.context),
        risk_budget: task.risk_budget,
        cursor: task.cursor,
        previous_response_id: task.responseId,
      },
    };
    if (typeof this.model.respond === "function") return this.model.respond(envelope);
    return this.model.propose(envelope);
  }

  async step(taskId) {
    const task = this._task(taskId);
    if (["completed", "failed", "cancelled"].includes(task.state)) return this.snapshot(taskId);
    if (task.state === "awaiting_approval" || task.state === "paused") return this.snapshot(taskId);

    this._heartbeat(task, true);
    if (!task.plan) {
      let rawResponse;
      try {
        rawResponse = await this._requestModel(task);
      } catch (error) {
        return this._fail(task, "model_error", { message: String(error && (error.message || error)) });
      }
      const normalized = normalizeModelResponse(rawResponse, task.task_id, task.turn);
      if (!normalized.ok) return this._fail(task, "invalid_model_response", normalized.errors);
      if (task.responseId === normalized.response.response_id) return this._fail(task, "duplicate_model_response", { response_id: task.responseId });
      task.responseId = normalized.response.response_id;
      task.plan = normalized.plan;
      task.turn += 1;
      this._event(task, "model.response", {
        response_id: task.responseId,
        turn: task.turn,
        action_count: task.plan.actions.length,
        digest: digest(task.plan),
        finish_reason: normalized.response.finish_reason || "tool_calls",
      });
      this._save(task);
      if (task.state === "paused") return this.snapshot(taskId);
    }

    while (task.cursor < task.plan.actions.length) {
      if (task.state === "paused") return this.snapshot(taskId);
      const action = task.plan.actions[task.cursor];
      let decision;
      try {
        decision = await this.policy.check(action, task);
      } catch (error) {
        return this._fail(task, "policy_error", { message: String(error && (error.message || error)) });
      }
      if (!decision || !["allow", "deny", "confirm"].includes(decision.decision)) return this._fail(task, "invalid_policy_decision");
      if (task.approvedActionId === action.action_id && decision.decision === "confirm") {
        decision = { ...decision, decision: "allow", reason: "one-time approval consumed" };
        task.approvedActionId = null;
      }
      if (decision.decision === "deny") return this._fail(task, "policy_denied", { action_id: action.action_id, reason: decision.reason });
      if (decision.decision === "confirm") {
        if (typeof this.policy.issueConfirmation !== "function") return this._fail(task, "policy_missing_token_issuer", { action_id: action.action_id });
        let token;
        try { token = await this.policy.issueConfirmation(action, task); } catch (error) { return this._fail(task, "approval_error", { message: String(error && (error.message || error)) }); }
        task.confirmation = { action_id: action.action_id, token };
        task.state = "awaiting_approval";
        this._event(task, "approval.requested", { action_id: action.action_id, risk: decision.risk, reason: decision.reason });
        this._save(task);
        return this.snapshot(taskId);
      }

      const prepared = { phase: "prepare", action_id: action.action_id, idempotency_key: action.idempotency_key };
      task.journal.push(prepared);
      this._event(task, "action.prepare", { action_id: action.action_id, idempotency_key: action.idempotency_key });
      this._save(task);
      let result;
      try {
        result = await this.executor.execute(action, { task_id: task.task_id, context: clone(task.context), response_id: task.responseId });
      } catch (error) {
        prepared.phase = "rollback_pending";
        prepared.error = String(error && (error.message || error));
        this._save(task);
        return this._fail(task, "executor_error", { action_id: action.action_id, message: prepared.error });
      }
      prepared.phase = "commit";
      prepared.result = clone(result);
      task.cursor += 1;
      this._event(task, "action.result", { action_id: action.action_id, ok: result && result.ok !== false, result: clone(result) });
      this._save(task);
      if (!result || result.ok === false) return this._fail(task, "action_failed", { action_id: action.action_id });
      if (task.state === "paused") return this.snapshot(taskId);
      this._heartbeat(task);
    }

    task.state = "completed";
    this._event(task, "task.completed", { response_id: task.responseId, actions: task.cursor });
    this._save(task);
    return this.snapshot(taskId);
  }

  async approve(taskId, { token } = {}) {
    const task = this._task(taskId);
    if (task.state !== "awaiting_approval" || !task.confirmation) throw new Error("task is not awaiting approval");
    const action = task.plan.actions[task.cursor];
    if (typeof this.policy.consumeConfirmation !== "function" || token !== task.confirmation.token || !(await this.policy.consumeConfirmation(token, action, task))) throw new Error("approval token rejected by policy");
    task.confirmation = null;
    task.approvedActionId = action.action_id;
    task.state = "running";
    this._event(task, "approval.granted", { action_id: action.action_id });
    this._save(task);
    return this.step(taskId);
  }

  async resume(taskId, options = {}) {
    return this.approve(taskId, options);
  }

  ackThrough(taskId, sequence) {
    const task = this._task(taskId);
    if (!Number.isInteger(sequence) || sequence < -1) throw new TypeError("sequence must be an integer >= -1");
    task.pendingEvents = task.pendingEvents.filter((event) => event.sequence > sequence);
    if (task.state === "paused" && task.pauseReason === "backpressure" && task.pendingEvents.length < this.maxOutstandingEvents) {
      task.state = "running";
      task.pauseReason = null;
      this._event(task, "runtime.resumed", { after_sequence: sequence });
      this._save(task);
    }
    return this.snapshot(taskId);
  }

  monitor(taskId) {
    const task = this._task(taskId);
    if (!ACTIVE_STATES.has(task.state) || task.state === "awaiting_approval") return this.snapshot(taskId);
    const age = this.nowMs() - task.lastHeartbeatMs;
    if (age > this.heartbeatTimeoutMs) return this._fail(task, "heartbeat_timeout", { age_ms: age, timeout_ms: this.heartbeatTimeoutMs });
    return this.snapshot(taskId);
  }

  checkpoint(taskId) {
    const task = this._task(taskId);
    this._event(task, "task.checkpoint", { state: task.state, cursor: task.cursor, response_id: task.responseId });
    this._save(task);
    return this.snapshot(taskId);
  }

  recover(taskId) {
    if (this.tasks.has(taskId)) return this.snapshot(taskId);
    const saved = this.checkpointStore.load(taskId);
    if (!saved) return null;
    const task = {
      task_id: saved.task_id,
      context: clone(saved.context),
      risk_budget: saved.risk_budget,
      state: saved.state,
      pauseReason: saved.pause_reason || null,
      sequence: saved.sequence,
      turn: saved.turn,
      cursor: saved.cursor,
      plan: clone(saved.plan),
      responseId: saved.response_id || null,
      confirmation: clone(saved.confirmation),
      approvedActionId: saved.approved_action_id || null,
      journal: clone(saved.journal) || [],
      events: clone(saved.events) || [],
      pendingEvents: clone(saved.pending_events) || [],
      lastHeartbeatMs: saved.last_heartbeat_ms || this.nowMs(),
    };
    this.tasks.set(taskId, task);
    return this.snapshot(taskId);
  }

  cancel(taskId, reason = "cancelled") {
    const task = this._task(taskId);
    if (!["completed", "failed", "cancelled"].includes(task.state)) {
      task.state = "cancelled";
      task.pauseReason = reason;
      this._event(task, "task.cancelled", { reason });
      this._save(task);
    }
    return this.snapshot(taskId);
  }

  snapshot(taskId) {
    const task = this._task(taskId);
    const snapshot = this._snapshot(task);
    snapshot.pending_event_count = task.pendingEvents.length;
    snapshot.event_count = task.events.length;
    return snapshot;
  }

  jsonl(taskId) {
    return this._task(taskId).events.map((event) => JSON.stringify(event)).join("\n") + (this._task(taskId).events.length ? "\n" : "");
  }
}

module.exports = {
  MODEL_RESPONSE_TYPE,
  normalizeModelResponse,
  MemoryCheckpointStore,
  DurableCheckpointStore,
  PiAgentLoop,
};

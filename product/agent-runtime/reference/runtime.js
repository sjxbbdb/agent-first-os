"use strict";

const crypto = require("node:crypto");

/** @typedef {"L0"|"L1"|"L2"|"L3"} RiskLevel */
/** @typedef {"created"|"running"|"awaiting_confirmation"|"paused"|"failed"|"completed"|"cancelled"} TaskState */

const PROTOCOL_VERSION = 1;
const MAX_ACTIONS = 256;
const TOOL_ID_RE = /^[a-z0-9._-]+$/;

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/** Deterministic JSON used for local digests and idempotency keys. */
function canonicalJson(value) {
  if (value === null || typeof value !== "object") return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(",")}]`;
  return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`).join(",")}}`;
}

function digest(value) {
  return crypto.createHash("sha256").update(canonicalJson(value)).digest("hex");
}

function deriveIdempotencyKey(taskId, action) {
  const identity = {
    task_id: taskId,
    tool_id: action.tool_id,
    tool_version: action.tool_version,
    params: action.params,
  };
  return `sha256:${digest(identity)}`;
}

/**
 * Return a normalized copy whose idempotency keys are owned by the runtime.
 * Model supplied keys are deliberately ignored.
 */
function normalizeActionPlan(plan) {
  if (!isRecord(plan)) return plan;
  return {
    ...plan,
    actions: Array.isArray(plan.actions)
      ? plan.actions.map((action) => !isRecord(action) ? action : ({
          ...action,
          idempotency_key: deriveIdempotencyKey(plan.task_id, action),
        }))
      : plan.actions,
  };
}

/** Validate JSON-schema fields plus cross-action invariants not expressible in JSON Schema. */
function validateActionPlan(plan) {
  const errors = [];
  if (!isRecord(plan)) return { ok: false, errors: ["plan must be an object"] };
  if (plan.version !== PROTOCOL_VERSION) errors.push("version must be 1");
  if (typeof plan.task_id !== "string" || plan.task_id.length === 0) errors.push("task_id is required");
  if (!Array.isArray(plan.actions)) errors.push("actions must be an array");
  if (Array.isArray(plan.actions) && plan.actions.length > MAX_ACTIONS) errors.push(`actions must contain at most ${MAX_ACTIONS} items`);
  if (Object.keys(plan).some((key) => !["version", "task_id", "actions"].includes(key))) errors.push("plan contains unknown properties");
  const actionIds = new Set();
  for (const [index, action] of (Array.isArray(plan.actions) ? plan.actions : []).entries()) {
    const prefix = `actions[${index}]`;
    if (!isRecord(action)) { errors.push(`${prefix} must be an object`); continue; }
    for (const key of ["action_id", "tool_id", "tool_version", "params", "risk", "idempotency_key"]) {
      if (!(key in action)) errors.push(`${prefix}.${key} is required`);
    }
    if (typeof action.action_id !== "string" || action.action_id.length === 0) errors.push(`${prefix}.action_id must be non-empty`);
    else if (actionIds.has(action.action_id)) errors.push(`${prefix}.action_id must be unique`);
    else actionIds.add(action.action_id);
    if (typeof action.tool_id !== "string" || !TOOL_ID_RE.test(action.tool_id)) errors.push(`${prefix}.tool_id is invalid`);
    if (typeof action.tool_version !== "string" || action.tool_version.length === 0) errors.push(`${prefix}.tool_version must be non-empty`);
    if (!isRecord(action.params)) errors.push(`${prefix}.params must be an object`);
    if (!["L0", "L1", "L2", "L3"].includes(action.risk)) errors.push(`${prefix}.risk must be L0, L1, L2 or L3`);
    if (typeof action.idempotency_key !== "string" || action.idempotency_key.length === 0) errors.push(`${prefix}.idempotency_key must be non-empty`);
    if (action.expected !== undefined && (!Array.isArray(action.expected) || action.expected.some((item) => !isRecord(item)))) errors.push(`${prefix}.expected must be an array of objects`);
    if (Object.keys(action).some((key) => !["action_id", "tool_id", "tool_version", "params", "risk", "idempotency_key", "expected"].includes(key))) errors.push(`${prefix} contains unknown properties`);
  }
  return { ok: errors.length === 0, errors };
}

class MockModelAdapter {
  constructor(plans = {}) {
    this.plans = plans instanceof Map ? new Map(plans) : new Map(Object.entries(plans));
    this.calls = [];
  }

  async propose(envelope) {
    this.calls.push({ task_id: envelope.task_id, sequence: envelope.sequence });
    const plan = this.plans.get(envelope.task_id);
    if (!plan) return { version: PROTOCOL_VERSION, task_id: envelope.task_id, actions: [] };
    return typeof plan === "function" ? await plan(envelope) : structuredClone(plan);
  }
}

/** Test fixture only: production policy must resolve tool capabilities and resources. */
class FixturePolicy {
  constructor({ requireConfirmationAt = "L2", toolRisks = {} } = {}) {
    this.requireConfirmationAt = requireConfirmationAt;
    this.toolRisks = new Map(Object.entries(toolRisks));
    this.tokens = new Map();
  }

  async check(action) {
    const rank = { L0: 0, L1: 1, L2: 2, L3: 3 };
    const risk = this.toolRisks.get(action.tool_id);
    if (!(risk in rank)) return { decision: "deny", risk: "L3", reason: "tool is not registered" };
    const threshold = rank[this.requireConfirmationAt];
    if (rank[risk] >= threshold) return { decision: "confirm", risk, reason: "confirmation required" };
    return { decision: "allow", risk, reason: "registered tool risk" };
  }

  async issueConfirmation(action, task) {
    const token = `fixture:${crypto.randomBytes(18).toString("hex")}`;
    this.tokens.set(token, { task_id: task.task_id, action_id: action.action_id, action_digest: digest(action) });
    return token;
  }

  async consumeConfirmation(token, action, task) {
    const binding = this.tokens.get(token);
    if (!binding || binding.task_id !== task.task_id || binding.action_id !== action.action_id || binding.action_digest !== digest(action)) return false;
    this.tokens.delete(token);
    return true;
  }
}

class AgentEngine {
  constructor({ model, policy, executor, now = () => new Date().toISOString(), onEvent = () => {} } = {}) {
    if (!model || typeof model.propose !== "function") throw new TypeError("model.propose is required");
    if (!policy || typeof policy.check !== "function") throw new TypeError("policy.check is required");
    if (!executor || typeof executor.execute !== "function") throw new TypeError("executor.execute is required");
    this.model = model; this.policy = policy; this.executor = executor; this.now = now; this.onEvent = onEvent;
    this.tasks = new Map();
  }

  _event(task, type, payload = {}) {
    const event = { version: PROTOCOL_VERSION, type, task_id: task.task_id, sequence: task.sequence++, payload, at: this.now() };
    task.events.push(event); this.onEvent(event); return event;
  }

  _task(taskId) {
    const task = this.tasks.get(taskId);
    if (!task) throw new Error(`task not found: ${taskId}`);
    return task;
  }

  async start({ task_id, context = {}, risk_budget = "L1" } = {}) {
    if (typeof task_id !== "string" || task_id.length === 0) throw new TypeError("task_id is required");
    if (this.tasks.has(task_id)) throw new Error(`task already exists: ${task_id}`);
    const task = { task_id, context, risk_budget, state: "created", sequence: 0, events: [], plan: null, cursor: 0, journal: [], confirmation: null };
    this.tasks.set(task_id, task);
    this._event(task, "task.open", { context, risk_budget });
    task.state = "running";
    this._event(task, "task.event", { state: task.state });
    return this.step(task_id);
  }

  async step(taskId) {
    const task = this._task(taskId);
    if (["completed", "failed", "cancelled"].includes(task.state)) return this.snapshot(taskId);
    if (task.state === "paused") throw new Error("task is paused; call resume first");
    if (task.state === "awaiting_confirmation") return this.snapshot(taskId);
    if (!task.plan) {
      let rawPlan;
      try { rawPlan = await this.model.propose({ version: PROTOCOL_VERSION, type: "task.context", task_id: task.task_id, sequence: task.sequence, payload: task.context }); }
      catch (error) { return this._fail(task, "model_error", { message: String(error.message || error) }); }
      const validation = validateActionPlan(rawPlan);
      if (!validation.ok || rawPlan.task_id !== task.task_id) return this._fail(task, "invalid_action_plan", validation.errors);
      const normalized = normalizeActionPlan(rawPlan);
      task.plan = normalized;
      this._event(task, "plan.proposed", { action_count: normalized.actions.length, digest: digest(normalized) });
    }
    while (task.cursor < task.plan.actions.length) {
      const action = task.plan.actions[task.cursor];
      const confirmed = task.confirmedActionId === action.action_id;
      if (confirmed) task.confirmedActionId = null;
      let decision;
      try { decision = await this.policy.check(action, task); }
      catch (error) { return this._fail(task, "policy_error", { message: String(error.message || error) }); }
      if (!["allow", "deny", "confirm"].includes(decision && decision.decision)) return this._fail(task, "invalid_policy_decision");
      if (confirmed && decision.decision === "confirm") decision = { ...decision, decision: "allow", reason: "one-time confirmation consumed" };
      this._event(task, "task.event", { state: decision.decision === "confirm" ? "awaiting_confirmation" : "running", action_id: action.action_id, policy: decision });
      if (decision.decision === "deny") return this._fail(task, "policy_denied", { action_id: action.action_id, reason: decision.reason });
      if (decision.decision === "confirm") {
        task.state = "awaiting_confirmation";
        if (typeof this.policy.issueConfirmation !== "function") return this._fail(task, "policy_missing_token_issuer", { action_id: action.action_id });
        const token = await this.policy.issueConfirmation(action, task);
        task.confirmation = { action_id: action.action_id, token };
        this._event(task, "task.event", { state: task.state, action_id: action.action_id, token_required: true });
        return this.snapshot(taskId);
      }
      task.state = "running";
      let result;
      try { result = await this.executor.execute(action, { task_id: task.task_id, context: task.context }); }
      catch (error) { return this._fail(task, "executor_error", { action_id: action.action_id, message: String(error.message || error) }); }
      task.journal.push({ action_id: action.action_id, idempotency_key: action.idempotency_key, result });
      this._event(task, "action.result", { action_id: action.action_id, ok: result && result.ok !== false, result });
      if (!result || result.ok === false) return this._fail(task, "action_failed", { action_id: action.action_id });
      task.cursor += 1;
    }
    task.state = "completed";
    this._event(task, "task.event", { state: task.state });
    return this.snapshot(taskId);
  }

  async resume(taskId, { token } = {}) {
    const task = this._task(taskId);
    if (task.state !== "awaiting_confirmation") throw new Error("task is not awaiting confirmation");
    if (!task.confirmation || token !== task.confirmation.token) throw new Error("invalid confirmation token");
    const action = task.plan.actions[task.cursor];
    if (typeof this.policy.consumeConfirmation !== "function" || !(await this.policy.consumeConfirmation(token, action, task))) throw new Error("confirmation token rejected by policy");
    task.confirmedActionId = task.confirmation.action_id;
    task.confirmation = null; task.state = "running";
    return this.step(taskId);
  }

  async cancel(taskId, reason = "cancelled") {
    const task = this._task(taskId);
    if (!["completed", "failed", "cancelled"].includes(task.state)) { task.state = "cancelled"; this._event(task, "task.event", { state: task.state, reason }); }
    return this.snapshot(taskId);
  }

  checkpoint(taskId) {
    const task = this._task(taskId);
    this._event(task, "task.checkpoint", { state: task.state, cursor: task.cursor, plan_digest: task.plan ? digest(task.plan) : null });
    return this.snapshot(taskId);
  }

  _fail(task, code, details) {
    task.state = "failed"; this._event(task, "task.event", { state: task.state, code, details }); return this.snapshot(task.task_id);
  }

  snapshot(taskId) {
    const task = this._task(taskId);
    return { task_id: task.task_id, state: task.state, cursor: task.cursor, action_count: task.plan ? task.plan.actions.length : 0, confirmation: task.confirmation, journal: task.journal.slice(), events: task.events.slice() };
  }

  jsonl(taskId) { return this._task(taskId).events.map((event) => JSON.stringify(event)).join("\n") + (this._task(taskId).events.length ? "\n" : ""); }
}

module.exports = { PROTOCOL_VERSION, MAX_ACTIONS, canonicalJson, digest, deriveIdempotencyKey, normalizeActionPlan, validateActionPlan, MockModelAdapter, FixturePolicy, AgentEngine };

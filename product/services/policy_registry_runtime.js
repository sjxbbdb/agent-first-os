"use strict";

/*
 * Host-side G7/G8 reference control plane.
 *
 * This module deliberately does not issue kernel capabilities or perform
 * privileged I/O.  It freezes the user-mode contracts that the native Policy
 * service and kernel token syscalls must later implement.
 */

const crypto = require("node:crypto");
const fs = require("node:fs");
const { canonicalJson, digest } = require("../agent-runtime/reference/runtime");

const RISK_RANK = Object.freeze({ L0: 0, L1: 1, L2: 2, L3: 3 });
const TOOL_ID_RE = /^[a-z0-9._-]+$/;

function clone(value) {
  return structuredClone(value);
}

function requireRecord(value, name) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
}

function toolKey(toolId, version) {
  return `${toolId}@${version}`;
}

function validateToolDescriptor(tool) {
  const errors = [];
  if (!tool || typeof tool !== "object" || Array.isArray(tool)) return ["tool must be an object"];
  if (tool.version !== 1) errors.push("version must be 1");
  if (typeof tool.tool_id !== "string" || !TOOL_ID_RE.test(tool.tool_id)) errors.push("tool_id is invalid");
  if (typeof tool.tool_version !== "string" || tool.tool_version.length === 0) errors.push("tool_version is required");
  if (typeof tool.description !== "string" || tool.description.length === 0) errors.push("description is required");
  if (!tool.parameters || typeof tool.parameters !== "object" || Array.isArray(tool.parameters)) errors.push("parameters must be an object");
  if (!Array.isArray(tool.required_capabilities) || tool.required_capabilities.some((value) => typeof value !== "string" || value.length === 0)) errors.push("required_capabilities must be a string array");
  if (!Object.prototype.hasOwnProperty.call(RISK_RANK, tool.risk)) errors.push("risk must be L0, L1, L2 or L3");
  if (typeof tool.reversible !== "boolean") errors.push("reversible must be boolean");
  if (typeof tool.provider !== "string" || tool.provider.length === 0) errors.push("provider is required");
  if (typeof tool.verifier !== "string" || tool.verifier.length === 0) errors.push("verifier is required");
  return errors;
}

class SemanticRegistry {
  constructor({ bootstrapActor = "registry.bootstrap" } = {}) {
    this.bootstrapActor = bootstrapActor;
    this.tools = new Map();
    this.revocations = new Map();
  }

  register(tool, { actor = this.bootstrapActor } = {}) {
    if (actor !== this.bootstrapActor) throw new Error("registry registration requires bootstrap actor");
    const errors = validateToolDescriptor(tool);
    if (errors.length) throw new Error(`invalid tool descriptor: ${errors.join(", ")}`);
    const key = toolKey(tool.tool_id, tool.tool_version);
    if (this.tools.has(key)) throw new Error(`tool already registered: ${key}`);
    this.tools.set(key, clone(tool));
    return clone(tool);
  }

  revoke(toolId, version, { actor = this.bootstrapActor, reason = "revoked" } = {}) {
    if (actor !== this.bootstrapActor) throw new Error("registry revoke requires bootstrap actor");
    const key = toolKey(toolId, version);
    if (!this.tools.has(key)) throw new Error(`tool not registered: ${key}`);
    this.revocations.set(key, { reason, at: new Date().toISOString() });
  }

  resolve(toolId, version) {
    const key = toolKey(toolId, version);
    const tool = this.tools.get(key);
    if (!tool || this.revocations.has(key)) return null;
    return clone(tool);
  }

  query({ capability, risk, provider } = {}) {
    return [...this.tools.entries()]
      .filter(([key, tool]) => !this.revocations.has(key))
      .filter(([, tool]) => capability === undefined || tool.required_capabilities.includes(capability))
      .filter(([, tool]) => risk === undefined || tool.risk === risk)
      .filter(([, tool]) => provider === undefined || tool.provider === provider)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([, tool]) => clone(tool));
  }

  validateParams(tool, params) {
    const errors = [];
    if (!params || typeof params !== "object" || Array.isArray(params)) return ["params must be an object"];
    const schema = tool.parameters || {};
    for (const required of schema.required || []) {
      if (!(required in params)) errors.push(`missing parameter: ${required}`);
    }
    for (const [name, descriptor] of Object.entries(schema.properties || {})) {
      if (!(name in params) || !descriptor || typeof descriptor !== "object") continue;
      const expected = descriptor.type;
      if (expected === "string" && typeof params[name] !== "string") errors.push(`${name} must be string`);
      if (expected === "integer" && (!Number.isInteger(params[name]) || params[name] < 0)) errors.push(`${name} must be non-negative integer`);
      if (expected === "boolean" && typeof params[name] !== "boolean") errors.push(`${name} must be boolean`);
    }
    return errors;
  }
}

class ContextCollector {
  capture({ currentWindow, authorizedFiles = [], fileContents = {} } = {}) {
    if (typeof currentWindow !== "string" || currentWindow.length === 0) throw new Error("current task window is required");
    if (!Array.isArray(authorizedFiles) || authorizedFiles.some((file) => typeof file !== "string" || file.length === 0)) throw new Error("authorizedFiles must be a string array");
    const files = {};
    for (const file of [...new Set(authorizedFiles)].sort()) {
      if (Object.prototype.hasOwnProperty.call(fileContents, file)) files[file] = fileContents[file];
    }
    return { current_window: currentWindow, authorized_files: [...new Set(authorizedFiles)].sort(), files };
  }
}

class PolicyFirewall {
  constructor({ registry, now = () => Date.now(), tokenTtlMs = 30_000, tokenBytes = 18 } = {}) {
    if (!registry || typeof registry.resolve !== "function") throw new TypeError("registry is required");
    this.registry = registry;
    this.now = now;
    this.tokenTtlMs = tokenTtlMs;
    this.tokenBytes = tokenBytes;
    this.tasks = new Map();
    this.tokens = new Map();
    this.capabilities = new Map();
    this.auditCounter = 0;
  }

  bindTask(taskId, { capabilities = [], authorizedFiles = [], riskBudget = "L1" } = {}) {
    if (typeof taskId !== "string" || taskId.length === 0) throw new Error("taskId is required");
    if (!Object.prototype.hasOwnProperty.call(RISK_RANK, riskBudget)) throw new Error("invalid risk budget");
    const caps = new Map();
    for (const cap of capabilities) {
      if (!cap || typeof cap.handle !== "string" || typeof cap.id !== "string") throw new Error("invalid capability binding");
      if (!Number.isInteger(cap.generation) || cap.generation < 1) throw new Error("invalid capability generation");
      const record = { handle: cap.handle, id: cap.id, generation: cap.generation, rights: [...new Set(cap.rights || [])].sort(), revoked: false };
      caps.set(record.handle, record);
      this.capabilities.set(record.handle, record);
    }
    this.tasks.set(taskId, { taskId, capabilities: caps, authorizedFiles: new Set(authorizedFiles), riskBudget });
  }

  revokeCapability(handle) {
    const cap = this.capabilities.get(handle);
    if (!cap) return false;
    cap.revoked = true;
    return true;
  }

  _task(task) {
    const bound = this.tasks.get(task.task_id);
    if (!bound) throw new Error("task is not bound to Policy Firewall");
    return bound;
  }

  _capabilityDigest(bound, required) {
    const selected = [];
    for (const requiredId of required) {
      const matches = [...bound.capabilities.values()].filter((cap) => cap.id === requiredId && !cap.revoked && cap.rights.includes("invoke"));
      if (matches.length === 0) return null;
      selected.push(...matches);
    }
    return `sha256:${digest(selected.sort((a, b) => a.handle.localeCompare(b.handle)))}`;
  }

  _resourceDigest(action) {
    return `sha256:${digest({ tool_id: action.tool_id, tool_version: action.tool_version, params: action.params })}`;
  }

  _decision(task, decision, risk, reason, action, capDigest, resourceDigest) {
    this.auditCounter += 1;
    return {
      version: 1,
      task_id: task.task_id,
      action_id: action.action_id,
      decision,
      risk,
      resource_digest: resourceDigest,
      capability_digest: capDigest || `sha256:${"0".repeat(64)}`,
      expires_at: new Date(this.now() + this.tokenTtlMs).toISOString(),
      audit_id: `audit-${String(this.auditCounter).padStart(6, "0")}`,
      reason,
    };
  }

  async check(action, task) {
    const bound = this._task(task);
    const tool = this.registry.resolve(action.tool_id, action.tool_version);
    const resourceDigest = this._resourceDigest(action);
    if (!tool) return this._decision(task, "deny", "L3", "tool is unknown or revoked", action, null, resourceDigest);
    const parameterErrors = this.registry.validateParams(tool, action.params);
    if (parameterErrors.length) return this._decision(task, "deny", tool.risk, `parameter validation failed: ${parameterErrors.join("; ")}`, action, null, resourceDigest);
    const capDigest = this._capabilityDigest(bound, tool.required_capabilities);
    if (!capDigest) return this._decision(task, "deny", tool.risk, "required capability is absent or revoked", action, null, resourceDigest);
    const path = action.params && action.params.path;
    if (typeof path === "string" && !bound.authorizedFiles.has(path)) return this._decision(task, "deny", tool.risk, "resource is outside explicit task authorization", action, capDigest, resourceDigest);
    if (RISK_RANK[tool.risk] > RISK_RANK[bound.riskBudget]) return this._decision(task, "deny", tool.risk, "risk exceeds task budget", action, capDigest, resourceDigest);
    const decision = RISK_RANK[tool.risk] >= RISK_RANK.L2 ? "confirm" : "allow";
    return this._decision(task, decision, tool.risk, `registry risk ${tool.risk}; model risk is advisory`, action, capDigest, resourceDigest);
  }

  async issueConfirmation(action, task) {
    const decision = await this.check(action, task);
    if (decision.decision !== "confirm") throw new Error(`confirmation is not required: ${decision.decision}`);
    const token = `policy:${crypto.randomBytes(this.tokenBytes).toString("hex")}`;
    this.tokens.set(token, {
      task_id: task.task_id,
      action_id: action.action_id,
      action_digest: digest(action),
      resource_digest: decision.resource_digest,
      capability_digest: decision.capability_digest,
      expires_at_ms: this.now() + this.tokenTtlMs,
      revoked: false,
    });
    return token;
  }

  revokeToken(token) {
    const binding = this.tokens.get(token);
    if (!binding) return false;
    binding.revoked = true;
    return true;
  }

  async consumeConfirmation(token, action, task) {
    const binding = this.tokens.get(token);
    if (!binding || binding.revoked || binding.expires_at_ms <= this.now()) return false;
    const decision = await this.check(action, task);
    const valid = binding.task_id === task.task_id && binding.action_id === action.action_id && binding.action_digest === digest(action) && binding.resource_digest === decision.resource_digest && binding.capability_digest === decision.capability_digest;
    if (!valid || decision.decision !== "confirm") return false;
    this.tokens.delete(token);
    return true;
  }
}

class Journal {
  constructor() { this.records = []; }
  append(record) {
    const entry = { sequence: this.records.length, ...clone(record) };
    this.records.push(entry);
    return entry;
  }
  jsonl() { return this.records.map((record) => JSON.stringify(record)).join("\n") + (this.records.length ? "\n" : ""); }
  recover() {
    const byAction = new Map();
    for (const record of this.records) {
      if (!record.action_id) continue;
      const current = byAction.get(record.action_id) || { action_id: record.action_id, state: "unknown" };
      if (record.phase === "prepare") current.state = "prepared";
      if (record.phase === "commit") current.state = "committed";
      if (record.phase === "rollback_pending") current.state = "rollback_pending";
      byAction.set(record.action_id, current);
    }
    return [...byAction.values()];
  }
}

/*
 * Host-side durable journal.  This gives the policy runtime a recoverable
 * append log; it is deliberately not presented as a disk transaction.
 * Every append is a complete JSONL record followed by fsync.  A reopen that
 * cannot prove the whole file is valid fails closed instead of guessing what
 * the last action meant.
 */
class DurableJSONLJournal extends Journal {
  constructor({ file } = {}) {
    if (typeof file !== "string" || file.length === 0) throw new TypeError("journal file is required");
    super();
    this.file = file;
    this._fd = fs.openSync(file, "a+");
    try {
      this.records = this._readAndValidate();
    } catch (error) {
      fs.closeSync(this._fd);
      this._fd = null;
      throw error;
    }
  }

  _readAndValidate() {
    const bytes = fs.readFileSync(this.file);
    if (bytes.length === 0) return [];
    let text;
    try {
      text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    } catch (error) {
      throw new Error(`journal contains invalid UTF-8: ${error.message}`);
    }
    if (!text.endsWith("\n")) throw new Error("journal tail is truncated (missing newline)");
    const records = [];
    for (const [index, line] of text.slice(0, -1).split("\n").entries()) {
      let record;
      try {
        record = JSON.parse(line);
      } catch (error) {
        throw new Error(`journal contains invalid JSON at line ${index + 1}: ${error.message}`);
      }
      if (!record || typeof record !== "object" || Array.isArray(record)) throw new Error(`journal record ${index} is not an object`);
      if (!Number.isInteger(record.sequence) || record.sequence !== index) throw new Error(`journal sequence mismatch at line ${index + 1}`);
      records.push(record);
    }
    return records;
  }

  append(record, { snapshot = undefined, preimage = undefined } = {}) {
    if (this._fd === null) throw new Error("journal is closed");
    requireRecord(record, "record");
    if (snapshot !== undefined) requireRecord(snapshot, "snapshot metadata");
    if (preimage !== undefined) requireRecord(preimage, "preimage metadata");
    const entry = { ...clone(record), sequence: this.records.length };
    if (snapshot !== undefined) entry.snapshot_metadata = clone(snapshot);
    if (preimage !== undefined) entry.preimage_metadata = clone(preimage);
    const bytes = Buffer.from(`${JSON.stringify(entry)}\n`, "utf8");
    let offset = 0;
    while (offset < bytes.length) offset += fs.writeSync(this._fd, bytes, offset, bytes.length - offset);
    fs.fsyncSync(this._fd);
    this.records.push(entry);
    return clone(entry);
  }

  getSnapshotMetadata(actionId) {
    const record = [...this.records].reverse().find((entry) => entry.action_id === actionId && entry.snapshot_metadata !== undefined);
    return record ? clone(record.snapshot_metadata) : null;
  }

  getPreimageMetadata(actionId) {
    const record = [...this.records].reverse().find((entry) => entry.action_id === actionId && entry.preimage_metadata !== undefined);
    return record ? clone(record.preimage_metadata) : null;
  }

  close() {
    if (this._fd !== null) {
      fs.closeSync(this._fd);
      this._fd = null;
    }
  }
}

class VerifiedExecutor {
  constructor({ registry, journal, execute }) {
    this.registry = registry;
    this.journal = journal;
    this.executeInner = execute;
  }

  async execute(action, context) {
    const tool = this.registry.resolve(action.tool_id, action.tool_version);
    if (!tool) return { ok: false, reason: "tool_revoked_before_execute" };
    this.journal.append({ phase: "prepare", task_id: context.task_id, action_id: action.action_id, idempotency_key: action.idempotency_key, resource_digest: `sha256:${digest({ tool_id: action.tool_id, tool_version: action.tool_version, params: action.params })}` });
    let result;
    try {
      result = await this.executeInner(action, context);
    } catch (error) {
      this.journal.append({ phase: "rollback_pending", task_id: context.task_id, action_id: action.action_id, reason: String(error.message || error), external_effect: "unknown" });
      return { ok: false, reason: "executor_error", message: String(error.message || error) };
    }
    let verified = Boolean(result && result.ok !== false);
    const expected = Array.isArray(action.expected) ? action.expected : [];
    for (const assertion of expected) {
      if (!assertion || typeof assertion.field !== "string" || !(assertion.field in (result || {})) || result[assertion.field] !== assertion.equals) verified = false;
    }
    if (!verified) {
      this.journal.append({ phase: "rollback_pending", task_id: context.task_id, action_id: action.action_id, reason: "postcondition_failed", external_effect: tool.reversible ? "reversible" : "unknown" });
      return { ok: false, reason: "postcondition_failed", verifier: tool.verifier };
    }
    this.journal.append({ phase: "commit", task_id: context.task_id, action_id: action.action_id, result: clone(result), verifier: tool.verifier });
    return result;
  }
}

module.exports = { RISK_RANK, validateToolDescriptor, SemanticRegistry, ContextCollector, PolicyFirewall, Journal, DurableJSONLJournal, VerifiedExecutor };

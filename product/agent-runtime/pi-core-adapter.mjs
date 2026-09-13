import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { PROTOCOL_VERSION, validateActionPlan } = require("./reference/runtime.js");

function emptyPlan(taskId) {
  return { version: PROTOCOL_VERSION, task_id: taskId, actions: [] };
}

function textFromMessage(message) {
  return (message?.content ?? [])
    .filter((part) => part?.type === "text" && typeof part.text === "string")
    .map((part) => part.text)
    .join("");
}

function parsePlan(text) {
  const trimmed = String(text ?? "").trim();
  const fenced = trimmed.match(/^```(?:json)?\s*([\s\S]*?)\s*```$/i);
  return JSON.parse(fenced ? fenced[1] : trimmed);
}

function checkedPlan(plan, taskId) {
  const result = validateActionPlan(plan);
  if (!result.ok) throw new Error(`Pi ActionPlan validation failed: ${result.errors.join("; ")}`);
  if (plan.task_id !== taskId) throw new Error("Pi ActionPlan task_id does not match envelope");
  return plan;
}

/**
 * Optional host-only bridge for an already configured pi-agent-core Agent.
 * The kernel never imports this module. Without an injected Agent, offline mode
 * remains dependency-free and returns the protocol-safe empty plan.
 */
export function createPiCoreAdapter({ agent, agentFactory, offline = false, onEvent } = {}) {
  return {
    async propose(envelope) {
      if (!envelope || typeof envelope.task_id !== "string" || envelope.task_id.length === 0) {
        throw new Error("Pi adapter requires an envelope.task_id");
      }
      if (offline) return emptyPlan(envelope.task_id);
      const resolved = agent ?? (agentFactory ? await agentFactory(envelope) : undefined);
      if (!resolved || typeof resolved.prompt !== "function" || typeof resolved.subscribe !== "function") {
        throw new Error("Pi adapter requires a configured Agent (or offline=true)");
      }
      let finalMessages;
      const unsubscribe = resolved.subscribe(async (event) => {
        onEvent?.(event);
        if (event?.type === "agent_end") finalMessages = event.messages;
      });
      try {
        await resolved.prompt(JSON.stringify(envelope));
      } finally {
        unsubscribe?.();
      }
      const last = [...(finalMessages ?? [])].reverse().find((message) => message?.role === "assistant");
      if (!last) throw new Error("Pi Agent produced no assistant message");
      return checkedPlan(parsePlan(textFromMessage(last)), envelope.task_id);
    },
  };
}

export { emptyPlan };

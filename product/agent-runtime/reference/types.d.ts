/** TypeScript declarations for the JavaScript reference runtime. */
export type RiskLevel = "L0" | "L1" | "L2" | "L3";
export type TaskState = "created" | "running" | "awaiting_confirmation" | "paused" | "failed" | "completed" | "cancelled";

export interface TaskEnvelope {
  version: 1;
  type: "task.context";
  task_id: string;
  sequence: number;
  payload: Record<string, unknown>;
}

export interface Action {
  action_id: string;
  tool_id: string;
  tool_version: string;
  params: Record<string, unknown>;
  risk: RiskLevel;
  idempotency_key: string;
  expected?: Record<string, unknown>[];
}

export interface ActionPlan {
  version: 1;
  task_id: string;
  actions: Action[];
}

export interface StructuredModelResponse {
  version: 1;
  type: "agent.plan";
  task_id: string;
  response_id: string;
  turn?: number;
  plan: ActionPlan;
  finish_reason?: string;
  usage?: Record<string, unknown>;
}

export interface PolicyDecision {
  decision: "allow" | "deny" | "confirm";
  risk: RiskLevel;
  reason: string;
}

export interface TaskEvent {
  version: 1;
  type: "task.open" | "task.context" | "plan.proposed" | "action.result" | "task.event" | "task.checkpoint";
  task_id: string;
  sequence: number;
  at: string;
  payload: Record<string, unknown>;
}

export interface ModelAdapter {
  propose(envelope: TaskEnvelope): Promise<ActionPlan>;
}

export interface StructuredModelAdapter {
  respond(envelope: TaskEnvelope & { turn: number }): Promise<StructuredModelResponse>;
}

export interface CheckpointStore {
  save(taskId: string, snapshot: Record<string, unknown>): void | Promise<void>;
  load(taskId: string): Record<string, unknown> | null | Promise<Record<string, unknown> | null>;
}

export interface Policy {
  check(action: Action, task: unknown): Promise<PolicyDecision>;
  issueConfirmation?(action: Action, task: unknown): Promise<string>;
  consumeConfirmation?(token: string, action: Action, task: unknown): Promise<boolean>;
}

export interface Executor {
  execute(action: Action, context: { task_id: string; context: Record<string, unknown> }): Promise<Record<string, unknown>>;
}

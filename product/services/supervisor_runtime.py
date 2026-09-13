"""Bounded, deterministic host-side Supervisor reference runtime.

This module models the JSON control-plane contract in ``supervisor-protocol.md``.
It deliberately does not create processes, capabilities, Ring 3 address spaces,
or QEMU devices; those are kernel/host integration concerns.
"""
from __future__ import annotations

import json
import re
import secrets
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Mapping, TextIO

SERVICE_ID = re.compile(r"^[a-z][a-z0-9._-]{0,63}$")
EVENTS = {"STARTING", "READY", "HEARTBEAT", "FAULT", "EXIT", "FROZEN", "RESTARTING", "STOPPING"}
RIGHTS = {"read", "write", "execute", "map", "send", "receive", "transfer", "inspect"}
PROTECTED = {"kernel", "kernel.admin", "policy.root", "security.root"}


class SupervisorError(ValueError):
    """Invalid manifest or lifecycle operation."""


class DependencyCycleError(SupervisorError):
    """The manifest dependency graph is not a DAG."""


class UnknownServiceError(SupervisorError):
    pass


@dataclass
class Service:
    manifest: dict
    state: str = "EXIT"
    generation: int = 0
    sequence: int = 0
    nonce: str | None = None
    started_ns: int | None = None
    last_heartbeat_ns: int | None = None
    restart_attempts: int = 0
    restart_due_ns: int | None = None
    capability_handles: tuple[str, ...] = ()

    @property
    def service_id(self) -> str:
        return self.manifest["service_id"]

    @property
    def task_group(self) -> str:
        return self.manifest.get("task_group") or self.service_id


def _check_manifest(manifest: Mapping) -> dict:
    if not isinstance(manifest, Mapping):
        raise SupervisorError("manifest must be an object")
    item = dict(manifest)
    required = {"version", "service_id", "entrypoint", "dependencies", "capabilities", "heartbeat", "restart"}
    missing = required - set(item)
    if missing:
        raise SupervisorError("manifest missing: " + ",".join(sorted(missing)))
    if set(item) - required - {"argv", "task_group"}:
        raise SupervisorError("manifest has unknown fields")
    if item["version"] != 1 or not isinstance(item["service_id"], str) or not SERVICE_ID.fullmatch(item["service_id"]):
        raise SupervisorError("invalid service_id/version")
    if not isinstance(item["entrypoint"], str) or not re.fullmatch(r"/[A-Za-z0-9._/@+\-]{1,191}", item["entrypoint"]):
        raise SupervisorError("entrypoint must be absolute")
    deps = item["dependencies"]
    if not isinstance(deps, list) or len(deps) > 32 or any(not isinstance(d, str) or not SERVICE_ID.fullmatch(d) for d in deps):
        raise SupervisorError("dependencies must be a unique list")
    if len(deps) != len(set(deps)):
        raise SupervisorError("dependencies must be a unique list")
    if item["service_id"] in deps:
        raise SupervisorError("service cannot depend on itself")
    if "argv" in item and (not isinstance(item["argv"], list) or len(item["argv"]) > 32 or any(not isinstance(arg, str) or len(arg) > 256 for arg in item["argv"])):
        raise SupervisorError("invalid argv")
    hb = item["heartbeat"]
    if not isinstance(hb, Mapping) or set(hb) != {"interval_ms", "timeout_ms", "grace_ms"}:
        raise SupervisorError("heartbeat fields are incomplete")
    if any(not isinstance(hb[k], int) or isinstance(hb[k], bool) or hb[k] < 0 for k in hb):
        raise SupervisorError("heartbeat values must be integers")
    if hb["interval_ms"] < 100 or hb["interval_ms"] > 3600000 or hb["timeout_ms"] < 100 or hb["timeout_ms"] > 3600000 or hb["grace_ms"] > 3600000:
        raise SupervisorError("heartbeat value outside schema range")
    if hb["timeout_ms"] < hb["interval_ms"]:
        raise SupervisorError("heartbeat timeout must be >= interval")
    restart = item["restart"]
    if not isinstance(restart, Mapping) or set(restart) != {"mode", "max_attempts", "backoff_ms"}:
        raise SupervisorError("restart fields are incomplete")
    if restart["mode"] not in {"never", "on-fault", "always"} or not isinstance(restart["max_attempts"], int) or isinstance(restart["max_attempts"], bool) or restart["max_attempts"] < 0 or restart["max_attempts"] > 32 or not isinstance(restart["backoff_ms"], int) or isinstance(restart["backoff_ms"], bool) or restart["backoff_ms"] < 0 or restart["backoff_ms"] > 3600000:
        raise SupervisorError("invalid restart policy")
    if restart["mode"] == "never" and restart["max_attempts"] != 0:
        raise SupervisorError("restart never must have max_attempts=0")
    if "task_group" in item and (not isinstance(item["task_group"], str) or not SERVICE_ID.fullmatch(item["task_group"])):
        raise SupervisorError("invalid task_group")
    if not isinstance(item["capabilities"], list) or len(item["capabilities"]) > 64:
        raise SupervisorError("invalid capabilities")
    resources: set[str] = set()
    for cap in item["capabilities"]:
        if not isinstance(cap, Mapping) or set(cap) != {"resource", "rights"} or not isinstance(cap["resource"], str):
            raise SupervisorError("invalid capability")
        if not re.fullmatch(r"[a-z][a-z0-9._:/@+\-]{0,191}", cap["resource"]):
            raise SupervisorError("invalid capability resource")
        if cap["resource"].lower() in PROTECTED:
            raise SupervisorError("manifest requests a protected root capability")
        rights = cap["rights"]
        if not isinstance(rights, list) or len(rights) > 8 or not rights or any(not isinstance(right, str) for right in rights):
            raise SupervisorError("invalid capability rights")
        if len(rights) != len(set(rights)) or not set(rights) <= RIGHTS:
            raise SupervisorError("invalid capability rights")
        if cap["resource"] in resources:
            raise SupervisorError("duplicate capability resource")
        resources.add(cap["resource"])
    return item


class Supervisor:
    """A bounded service state machine with append-only JSONL evidence."""

    def __init__(
        self,
        manifests: Iterable[Mapping] | Mapping[str, Mapping],
        *,
        clock_ns: Callable[[], int] | None = None,
        nonce_factory: Callable[[], str] | None = None,
        event_log: str | Path | TextIO | None = None,
        event_sink: Callable[[dict], None] | None = None,
    ) -> None:
        values = manifests.values() if isinstance(manifests, Mapping) else manifests
        self.services: dict[str, Service] = {}
        capability_owners: dict[str, str] = {}
        for raw in values:
            item = _check_manifest(raw)
            sid = item["service_id"]
            if sid in self.services:
                raise SupervisorError("duplicate service_id: " + sid)
            for capability in item["capabilities"]:
                resource = capability["resource"]
                owner = capability_owners.get(resource)
                if owner is not None:
                    raise SupervisorError(f"capability resource conflict: {resource} ({owner}, {sid})")
                capability_owners[resource] = sid
            self.services[sid] = Service(item)
        for service in self.services.values():
            missing = set(service.manifest["dependencies"]) - set(self.services)
            if missing:
                raise SupervisorError(f"{service.service_id} has unknown dependencies: {','.join(sorted(missing))}")
        self._topological_order = self._compute_order()
        self.clock_ns = clock_ns or __import__("time").monotonic_ns
        self.nonce_factory = nonce_factory or (lambda: secrets.token_hex(16))
        self.event_sink = event_sink
        self._event_file: TextIO | None = None
        self._event_path: Path | None = None
        if event_log is not None:
            if hasattr(event_log, "write"):
                self._event_file = event_log  # type: ignore[assignment]
            else:
                self._event_path = Path(event_log)
                self._event_file = self._event_path.open("a", encoding="utf-8")
        self._last_monotonic_ns = 0
        self._revoked_capabilities: set[str] = set()
        self._capabilities: dict[str, tuple[str, int, str, frozenset[str]]] = {}
        self._frozen_groups: set[str] = set()

    def close(self) -> None:
        if self._event_file is not None and self._event_path is not None:
            self._event_file.close()
            self._event_file = None

    def __enter__(self) -> "Supervisor":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def _compute_order(self) -> list[str]:
        indegree = {sid: len(s.manifest["dependencies"]) for sid, s in self.services.items()}
        users: dict[str, list[str]] = {sid: [] for sid in self.services}
        for sid, service in self.services.items():
            for dep in service.manifest["dependencies"]:
                users[dep].append(sid)
        ready = sorted(sid for sid, count in indegree.items() if count == 0)
        order: list[str] = []
        while ready:
            sid = ready.pop(0)
            order.append(sid)
            for child in sorted(users[sid]):
                indegree[child] -= 1
                if indegree[child] == 0:
                    ready.append(child)
                    ready.sort()
        if len(order) != len(self.services):
            raise DependencyCycleError("service dependency graph contains a cycle")
        return order

    def topological_order(self) -> tuple[str, ...]:
        return tuple(self._topological_order)

    def _service(self, sid: str) -> Service:
        try:
            return self.services[sid]
        except KeyError as exc:
            raise UnknownServiceError(sid) from exc

    def snapshot(self, sid: str | None = None) -> dict | dict[str, dict]:
        def one(s: Service) -> dict:
            return {"service_id": s.service_id, "state": s.state, "generation": s.generation, "sequence": s.sequence, "heartbeat_nonce": s.nonce, "restart_attempts": s.restart_attempts, "restart_due_ns": s.restart_due_ns, "task_group": s.task_group, "capability_handles": list(s.capability_handles)}
        if sid is not None:
            return one(self._service(sid))
        return {key: one(self.services[key]) for key in sorted(self.services)}

    def _emit(self, service: Service, event: str, **extra: object) -> dict:
        if event not in EVENTS:
            raise SupervisorError("unknown event")
        now = max(int(self.clock_ns()), self._last_monotonic_ns)
        self._last_monotonic_ns = now
        service.sequence += 1
        item: dict[str, object] = {"version": 1, "event": event, "service_id": service.service_id, "sequence": service.sequence, "monotonic_ns": now}
        if service.generation:
            item["generation"] = service.generation
        item.update(extra)
        if self.event_sink:
            self.event_sink(dict(item))
        if self._event_file:
            self._event_file.write(json.dumps(item, sort_keys=True, separators=(",", ":")) + "\n")
            self._event_file.flush()
        return item

    def _new_nonce(self) -> str:
        nonce = str(self.nonce_factory())
        if not re.fullmatch(r"[A-Fa-f0-9]{16,64}", nonce):
            raise SupervisorError("nonce_factory must return 16-64 hex characters")
        return nonce.lower()

    def start_all(self) -> tuple[str, ...]:
        for sid in self._topological_order:
            self.start(sid, _ignore_dependencies=True)
        return tuple(self._topological_order)

    def start(self, sid: str, *, _ignore_dependencies: bool = False) -> bool:
        service = self._service(sid)
        if service.task_group in self._frozen_groups:
            return False
        if not _ignore_dependencies and any(self._service(dep).state not in {"READY", "HEARTBEAT"} for dep in service.manifest["dependencies"]):
            return False
        was_restarting = service.state == "RESTARTING"
        if service.state in {"STARTING", "READY", "HEARTBEAT", "RESTARTING"}:
            if service.state == "RESTARTING" and service.restart_due_ns is not None and int(self.clock_ns()) < service.restart_due_ns:
                return False
            if service.state != "RESTARTING":
                return False
        if service.generation == 0:
            service.generation = 1
        service.state = "STARTING"
        if not was_restarting or service.nonce is None:
            service.nonce = self._new_nonce()
        now = int(self.clock_ns())
        service.started_ns = now
        service.last_heartbeat_ns = now
        service.restart_due_ns = None
        self._mint_capabilities(service)
        self._emit(service, "STARTING")
        return True

    def ready(self, sid: str, *, generation: int | None = None, nonce: str | None = None) -> bool:
        service = self._service(sid)
        if generation is not None and generation != service.generation or nonce is not None and nonce != service.nonce:
            return False
        if service.state != "STARTING":
            return False
        service.state = "READY"
        service.last_heartbeat_ns = int(self.clock_ns())
        self._emit(service, "READY")
        return True

    def heartbeat(self, sid: str, generation: int, nonce: str) -> bool:
        service = self._service(sid)
        if not isinstance(nonce, str):
            return False
        if generation != service.generation or nonce.lower() != (service.nonce or "").lower() or service.state in {"FAULT", "FROZEN", "EXIT", "RESTARTING"}:
            return False
        if service.state == "STARTING":
            self.ready(sid, generation=generation, nonce=nonce)
        service.state = "HEARTBEAT"
        service.last_heartbeat_ns = int(self.clock_ns())
        self._emit(service, "HEARTBEAT", heartbeat_nonce=service.nonce)
        return True

    def _freeze_group(self, group: str, *, exclude: str | None = None) -> tuple[str, ...]:
        # An explicit group pause blocks all future starts. Fault handling
        # freezes peers while allowing the failed service's scheduled restart.
        if exclude is None:
            self._frozen_groups.add(group)
        frozen: list[str] = []
        for service in self.services.values():
            if service.task_group != group or service.service_id == exclude or service.state == "EXIT":
                continue
            if service.state != "FROZEN":
                service.state = "FROZEN"
                self._emit(service, "FROZEN", reason="task_group")
            frozen.append(service.service_id)
        return tuple(sorted(frozen))

    def freeze_group(self, group: str) -> tuple[str, ...]:
        return self._freeze_group(group)

    def unfreeze_group(self, group: str) -> tuple[str, ...]:
        self._frozen_groups.discard(group)
        resumed: list[str] = []
        for service in self.services.values():
            if service.task_group == group and service.state == "FROZEN":
                service.state = "READY"
                service.last_heartbeat_ns = int(self.clock_ns())
                self._emit(service, "READY", reason="task_group_unfrozen")
                resumed.append(service.service_id)
        return tuple(sorted(resumed))

    def _schedule_restart(self, service: Service, reason: str) -> bool:
        policy = service.manifest["restart"]
        if policy["mode"] == "never" or service.restart_attempts >= policy["max_attempts"]:
            service.restart_due_ns = None
            return False
        self._revoke_capabilities(service)
        service.restart_attempts += 1
        service.generation += 1
        service.nonce = self._new_nonce()
        service.restart_due_ns = int(self.clock_ns()) + policy["backoff_ms"] * 1_000_000
        service.state = "RESTARTING"
        self._emit(service, "RESTARTING", reason=reason)
        return True

    def fault(self, sid: str, reason: str = "fault", *, exit_code: int | None = None) -> bool:
        service = self._service(sid)
        if service.state == "EXIT":
            return False
        service.state = "FAULT"
        self._revoke_capabilities(service)
        extra: dict[str, object] = {"reason": reason[:256]}
        if exit_code is not None:
            extra["exit_code"] = int(exit_code)
        self._emit(service, "FAULT", **extra)
        self._freeze_group(service.task_group, exclude=sid)
        restarted = self._schedule_restart(service, reason)
        if not restarted:
            service.state = "FAULT"
        return restarted

    def exit(self, sid: str, exit_code: int = 0, reason: str | None = None) -> bool:
        service = self._service(sid)
        if service.state == "EXIT":
            return False
        service.state = "EXIT"
        self._revoke_capabilities(service)
        extra: dict[str, object] = {"exit_code": int(exit_code)}
        if reason:
            extra["reason"] = reason[:256]
        self._emit(service, "EXIT", **extra)
        self._freeze_group(service.task_group, exclude=sid)
        return self._schedule_restart(service, reason or "exit") if service.manifest["restart"]["mode"] == "always" else False

    def stop(self, sid: str, reason: str = "stopped") -> bool:
        """Request a bounded stop; no automatic restart is scheduled."""
        service = self._service(sid)
        if service.state == "EXIT":
            return False
        self._emit(service, "STOPPING", reason=reason[:256])
        service.state = "EXIT"
        self._revoke_capabilities(service)
        self._emit(service, "EXIT", exit_code=0, reason=reason[:256])
        self._freeze_group(service.task_group, exclude=sid)
        return True

    def _mint_capabilities(self, service: Service) -> None:
        handles = []
        for index, cap in enumerate(service.manifest["capabilities"]):
            handle = f"{service.service_id}:{service.generation}:{index}"
            self._capabilities[handle] = (service.service_id, service.generation, cap["resource"], frozenset(cap["rights"]))
            handles.append(handle)
        service.capability_handles = tuple(handles)

    def _revoke_capabilities(self, service: Service) -> None:
        for handle in service.capability_handles:
            self._revoked_capabilities.add(handle)
            self._capabilities.pop(handle, None)
        service.capability_handles = ()

    def validate_capability(self, handle: str, service_id: str, right: str) -> bool:
        grant = self._capabilities.get(handle)
        return bool(grant and grant[0] == service_id and right in grant[3] and handle not in self._revoked_capabilities and grant[1] == self._service(service_id).generation)

    def tick(self) -> tuple[str, ...]:
        now = int(self.clock_ns())
        actions: list[str] = []
        for service in self.services.values():
            if service.state == "RESTARTING" and service.restart_due_ns is not None and now >= service.restart_due_ns:
                self.start(service.service_id)
                actions.append(service.service_id)
            elif service.state in {"STARTING", "READY", "HEARTBEAT"} and service.last_heartbeat_ns is not None:
                hb = service.manifest["heartbeat"]
                if now - service.last_heartbeat_ns >= (hb["timeout_ms"] + hb["grace_ms"]) * 1_000_000:
                    self.fault(service.service_id, "heartbeat_timeout")
                    actions.append(service.service_id)
        return tuple(sorted(actions))


__all__ = ["DependencyCycleError", "Service", "Supervisor", "SupervisorError", "UnknownServiceError"]

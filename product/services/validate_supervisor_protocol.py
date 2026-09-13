#!/usr/bin/env python3
"""Deterministic host-side checks for the Supervisor protocol fixtures."""
from __future__ import annotations

import hashlib
import json
import pathlib
import sys

SERVICE_ID = "^[a-z][a-z0-9._-]{0,63}$"
RIGHTS = {"read", "write", "execute", "map", "send", "receive", "transfer", "inspect"}
EVENTS = {"STARTING", "READY", "HEARTBEAT", "FAULT", "EXIT", "FROZEN", "RESTARTING", "STOPPING"}


def fail(message: str) -> None:
    raise ValueError(message)


def service(manifest: dict) -> None:
    required = {"version", "service_id", "entrypoint", "dependencies", "capabilities", "heartbeat", "restart"}
    missing = required - set(manifest)
    if missing:
        fail("manifest is missing: " + ",".join(sorted(missing)))
    if set(manifest) - required - {"argv", "task_group"}:
        fail("manifest has unknown fields")
    if manifest.get("version") != 1 or not isinstance(manifest.get("service_id"), str):
        fail("manifest version or service_id is invalid")
    import re
    if not re.fullmatch(SERVICE_ID, manifest["service_id"]):
        fail("manifest service_id is invalid")
    if not isinstance(manifest.get("entrypoint"), str) or not manifest["entrypoint"].startswith("/"):
        fail("manifest entrypoint must be absolute")
    deps = manifest["dependencies"]
    if not isinstance(deps, list) or len(deps) != len(set(deps)):
        fail("manifest dependencies must be a unique list")
    if manifest["service_id"] in deps:
        fail("service cannot depend on itself")
    hb = manifest["heartbeat"]
    if not isinstance(hb, dict) or not {"interval_ms", "timeout_ms", "grace_ms"} <= set(hb):
        fail("heartbeat fields are incomplete")
    if hb["interval_ms"] > hb["timeout_ms"]:
        fail("heartbeat timeout must be >= interval")
    restart = manifest["restart"]
    if not isinstance(restart, dict) or not {"mode", "max_attempts", "backoff_ms"} <= set(restart):
        fail("restart fields are incomplete")
    if restart["mode"] == "never" and restart["max_attempts"] != 0:
        fail("restart never must have max_attempts=0")
    for cap in manifest["capabilities"]:
        if not cap["rights"] or not set(cap["rights"]) <= RIGHTS:
            fail("capability rights are invalid")
        if cap["resource"].lower() in {"kernel", "kernel.admin", "policy.root", "security.root"}:
            fail("manifest requests a protected root capability")


def event(item: dict, previous: dict | None) -> None:
    if set(item) - {"version", "event", "service_id", "sequence", "monotonic_ns", "generation", "exit_code", "reason", "heartbeat_nonce"}:
        fail("event has unknown fields")
    if item.get("version") != 1 or item.get("event") not in EVENTS:
        fail("event version or type is invalid")
    if item["sequence"] < 0 or item["monotonic_ns"] < 0:
        fail("event counters must be non-negative")
    if previous:
        if item["service_id"] != previous["service_id"]:
            fail("event stream contains multiple services")
        if item["sequence"] <= previous["sequence"]:
            fail("event sequence is not strictly increasing")
        if item["monotonic_ns"] < previous["monotonic_ns"]:
            fail("event monotonic_ns moved backwards")
        if "generation" in item and "generation" in previous and item["generation"] < previous["generation"]:
            fail("event generation moved backwards")
    if item["event"] == "HEARTBEAT" and "heartbeat_nonce" not in item:
        fail("heartbeat requires heartbeat_nonce")


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} MANIFEST EVENT_JSONL", file=sys.stderr)
        return 2
    manifest_path, events_path = map(pathlib.Path, sys.argv[1:])
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    service(manifest)
    previous = None
    for line_no, line in enumerate(events_path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            item = json.loads(line)
            event(item, previous)
        except (json.JSONDecodeError, ValueError, KeyError, TypeError) as exc:
            fail(f"event line {line_no}: {exc}")
        previous = item
    canonical = json.dumps(manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    print("Supervisor protocol OK")
    print("manifest_sha256=" + hashlib.sha256(canonical).hexdigest())
    print(f"events={sum(1 for line in events_path.read_text(encoding='utf-8').splitlines() if line.strip())}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"Supervisor protocol FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)

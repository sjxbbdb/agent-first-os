#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python_bin="${PYTHON:-python3}"
"$python_bin" - "$repo_root" <<'PY'
import io
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
sys.path.insert(0, str(root / "product" / "services"))
from supervisor_runtime import DependencyCycleError, Supervisor

class Clock:
    def __init__(self): self.ns = 0
    def __call__(self): return self.ns
    def advance_ms(self, ms): self.ns += ms * 1_000_000

def manifest(sid, deps=(), group="workers", mode="on-fault", attempts=1, backoff=10):
    return {"version": 1, "service_id": sid, "entrypoint": "/sbin/" + sid,
            "dependencies": list(deps), "capabilities": [{"resource": "ipc:" + sid, "rights": ["send"]}],
            "heartbeat": {"interval_ms": 100, "timeout_ms": 200, "grace_ms": 0},
            "restart": {"mode": mode, "max_attempts": attempts, "backoff_ms": backoff},
            "task_group": group}

try:
    Supervisor([manifest("a", ["b"]), manifest("b", ["a"])])
except DependencyCycleError:
    pass
else:
    raise AssertionError("cycle was accepted")

clock = Clock(); out = io.StringIO(); nonces = iter(["a" * 16, "b" * 16, "c" * 16])
sup = Supervisor([manifest("base"), manifest("worker", ["base"])], clock_ns=clock, nonce_factory=lambda: next(nonces), event_log=out)
assert sup.topological_order() == ("base", "worker")
sup.start_all()
base_gen, base_nonce = sup.snapshot("base")["generation"], sup.snapshot("base")["heartbeat_nonce"]
assert sup.heartbeat("base", base_gen, base_nonce)
assert sup.snapshot("base")["state"] == "HEARTBEAT"
assert not sup.heartbeat("base", base_gen - 1, base_nonce), "stale generation accepted"
clock.advance_ms(200)
sup.tick()
assert sup.snapshot("base")["state"] == "RESTARTING"
restart_gen = sup.snapshot("base")["generation"]
assert restart_gen == base_gen + 1
clock.advance_ms(10)
sup.tick()
assert sup.snapshot("base")["state"] == "STARTING"
assert not sup.heartbeat("base", base_gen, base_nonce), "old heartbeat revived service"
new_nonce = sup.snapshot("base")["heartbeat_nonce"]
assert sup.heartbeat("base", restart_gen, new_nonce)
assert set(sup.freeze_group("workers")) == {"base", "worker"}
assert all(s["state"] == "FROZEN" for s in sup.snapshot().values())

events = [json.loads(line) for line in out.getvalue().splitlines() if line]
assert events and all(e["version"] == 1 for e in events)
for sid in {e["service_id"] for e in events}:
    seq = [e["sequence"] for e in events if e["service_id"] == sid]
    assert seq == list(range(1, len(seq) + 1))
print("Supervisor runtime OK")
PY

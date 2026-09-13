#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PYTHONPATH="$repo_root/product/services" python3 - <<'PY'
from supervisor_runtime import Supervisor
def m(sid, deps=(), group="g"):
    return {"version":1,"service_id":sid,"entrypoint":"/sbin/"+sid,"dependencies":list(deps),"capabilities":[{"resource":"ipc:"+sid,"rights":["send"]}],"heartbeat":{"interval_ms":100,"timeout_ms":200,"grace_ms":0},"restart":{"mode":"on-fault","max_attempts":1,"backoff_ms":0},"task_group":group}
s=Supervisor([m("base"),m("leaf",["base"])])
assert s.start("leaf") is False
assert s.start("base") is True
assert s.ready("base") is True
assert s.start("leaf") is True
old=s.snapshot("leaf")["capability_handles"][0]
assert s.validate_capability(old,"leaf","send")
s.fault("leaf")
assert not s.validate_capability(old,"leaf","send")
assert s.snapshot("leaf")["generation"] == 2
s.unfreeze_group("g")
s.tick()
new=s.snapshot("leaf")["capability_handles"][0]
assert new != old and s.validate_capability(new,"leaf","send")
s.freeze_group("g")
assert s.start("base") is False
print("Supervisor G4 group/DAG/capability lifecycle OK")
PY

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
printf '#include "supervisor_protocol.h"\n' | \
  gcc -std=c11 -ffreestanding -fsyntax-only -Wall -Wextra \
    -I"$repo_root/product/userland/include" -x c -
python3 "$repo_root/product/services/validate_supervisor_protocol.py" \
  "$repo_root/product/services/examples/echo.manifest.json" \
  "$repo_root/product/services/examples/echo.events.jsonl"

if python3 "$repo_root/product/services/validate_supervisor_protocol.py" \
  "$repo_root/product/services/examples/invalid-root-capability.manifest.json" \
  "$repo_root/product/services/examples/echo.events.jsonl"; then
  echo "protected root capability was accepted" >&2
  exit 1
fi
echo "negative manifest check OK"

python3 - "$repo_root/product/services" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / "supervisor-manifest.schema.json").read_text(encoding="utf-8"))
event = json.loads((root / "service-event.schema.json").read_text(encoding="utf-8"))
assert manifest["properties"]["version"]["const"] == 1
assert event["properties"]["event"]["enum"] == ["STARTING", "READY", "HEARTBEAT", "FAULT", "EXIT", "FROZEN", "RESTARTING", "STOPPING"]
print("Supervisor schemas OK")
PY

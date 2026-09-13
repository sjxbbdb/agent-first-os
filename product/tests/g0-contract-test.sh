#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
include_dir="$repo_root/product/kernel/include"
build_dir="$repo_root/build/g0"
mkdir -p "$build_dir"

cat > "$build_dir/abi-check.c" <<'EOF'
#include "abi.h"
#include "boot_info_v2.h"
#include "capability.h"
#include "ipc.h"
#include "syscall.h"
#include "service_protocol.h"
#include "virtio_protocol.h"
#include "shared_memory.h"
int main(void) { return (AGENT_OS_ABI_VERSION == 1 &&
                         AGENT_OS_BOOT_INFO_VERSION_V2 == 2) ? 0 : 1; }
EOF

gcc -std=c11 -ffreestanding -fsyntax-only -Wall -Wextra \
    -I"$include_dir" -I"$repo_root/product/userland/include" \
    -I"$repo_root/product/services" "$build_dir/abi-check.c"

python3 - "$repo_root/product/agent-runtime/protocol/action-plan.schema.json" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    schema = json.load(handle)
assert schema["$schema"].endswith("draft/2020-12/schema")
assert schema["properties"]["version"]["const"] == 1
assert schema["properties"]["actions"]["maxItems"] == 256
action = schema["properties"]["actions"]["items"]
assert action["additionalProperties"] is False
assert "idempotency_key" in action["required"]
print("G0 protocol schema OK")
PY

python3 - "$repo_root/product/agent-runtime/protocol/envelope.schema.json" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    schema = json.load(handle)
assert set(("version", "type", "task_id", "sequence", "payload")) <= set(schema["required"])
assert schema["properties"]["version"]["const"] == 1
assert schema["properties"]["sequence"]["minimum"] == 0
print("G0 envelope schema OK")
PY

python3 - "$repo_root/product/services/policy-firewall.schema.json" "$repo_root/product/services/semantic-tool.schema.json" <<'PY'
import json
import sys

policy, tool = [json.load(open(path, encoding="utf-8")) for path in sys.argv[1:]]
assert policy["properties"]["decision"]["enum"] == ["allow", "confirm", "deny", "pause"]
assert tool["properties"]["tool_id"]["pattern"].startswith("^[a-z0-9")
print("G0 policy and registry schemas OK")
PY

echo "G0 ABI headers OK"

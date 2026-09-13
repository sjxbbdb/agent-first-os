#!/usr/bin/env bash
set -euo pipefail
node "$(dirname "$0")/g9-pi-core-adapter.test.mjs"
if [[ -d "build/pi-adapter-smoke/node_modules/@earendil-works/pi-agent-core" ]]; then
  PI_CORE_SMOKE=1 node "$(dirname "$0")/g9-pi-core-adapter.test.mjs"
fi

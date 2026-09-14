#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cat <<'EOF' | gcc -std=c11 -ffreestanding -Wall -Wextra \
    -I"$repo_root/product/kernel/include" \
    -I"$repo_root/product/userland/include" -x c -o "$repo_root/build/g9-runtime-service-abi" -
#include <assert.h>
#include <string.h>
#include "agent_runtime_protocol.h"
int main(void) {
    AgentOsRuntimeEvent event = {0};
    event.header.version = AGENT_OS_ABI_VERSION;
    event.header.size = sizeof(event);
    event.opcode = AGENT_OS_RUNTIME_HEARTBEAT;
    event.length = 1;
    event.sequence = 7;
    event.words[0] = 7;
    assert(offsetof(AgentOsRuntimeEvent, sequence) == 24);
    assert(offsetof(AgentOsRuntimeEvent, words) == 32);
    assert(event.header.size == sizeof(IpcMessage));
    assert(AGENT_OS_RUNTIME_EVENT_SEQUENCE(event) == 7);
    assert(AGENT_OS_RUNTIME_EVENT_WORD(event, 0) == 7);
    return 0;
}
EOF
"$repo_root/build/g9-runtime-service-abi"
echo 'PASS: Agent Runtime native control ABI layout'

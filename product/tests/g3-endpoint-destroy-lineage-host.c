#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "capability_table.h"

int main(void) {
    AgentOsCapabilityTable source, sibling;
    CapabilityHandle root = 0, child = 0;
    uint64_t object = UINT64_C(0x12345000), out = 0;
    agent_os_capability_table_init(&source);
    agent_os_capability_table_init(&sibling);
    assert(agent_os_capability_mint(&source, object,
        CAP_RIGHT_SEND | CAP_RIGHT_TRANSFER | CAP_RIGHT_REVOKE, &root) == AGENT_OS_OK);
    assert(agent_os_capability_transfer(&source, root, &sibling,
        CAP_RIGHT_SEND, &child) == AGENT_OS_OK);
    assert(agent_os_capability_revoke_object(&source, object) == 1);
    assert(agent_os_capability_revoke_object(&sibling, object) == 1);
    assert(agent_os_capability_lookup(&source, root, CAP_RIGHT_SEND, &out, 0) != AGENT_OS_OK);
    assert(agent_os_capability_lookup(&sibling, child, CAP_RIGHT_SEND, &out, 0) != AGENT_OS_OK);
    CapabilityHandle replacement = 0;
    assert(agent_os_capability_mint(&source, object, CAP_RIGHT_SEND, &replacement) == AGENT_OS_OK);
    assert(replacement != root);
    puts("G3 endpoint destroy lineage OK: source and transferred sibling handles retired");
    return 0;
}

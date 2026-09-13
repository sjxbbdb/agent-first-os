#include <assert.h>
#include <stdio.h>

#include "capability_table.h"
#include "ipc_endpoint.h"

static IpcMessage message(uint32_t opcode) {
    IpcMessage value = {0};
    value.header.version = AGENT_OS_ABI_VERSION;
    value.header.size = sizeof(value);
    value.opcode = opcode;
    value.length = sizeof(value.words);
    value.words[0] = 0xfeed;
    return value;
}

int main(void) {
    AgentOsCapabilityTable owner;
    AgentOsCapabilityTable receiver;
    CapabilityHandle source;
    CapabilityHandle attenuated;
    CapabilityHandle copied;
    uint64_t object;
    uint64_t rights;

    assert(agent_os_capability_mint(0, 1, CAP_RIGHT_READ, &source) ==
           AGENT_OS_E_INVAL);
    agent_os_capability_table_init(&owner);
    agent_os_capability_table_init(&receiver);
    assert(agent_os_capability_mint(
               &owner, 0x1234, CAP_RIGHT_READ | CAP_RIGHT_TRANSFER |
                                CAP_RIGHT_REVOKE,
               &source) == AGENT_OS_OK);
    assert(agent_os_capability_restrict(&owner, source, CAP_RIGHT_READ,
                                       &attenuated) == AGENT_OS_OK);
    assert(agent_os_capability_lookup(&owner, attenuated, CAP_RIGHT_READ,
                                      &object, &rights) == AGENT_OS_OK);
    assert(object == 0x1234 && rights == CAP_RIGHT_READ);
    assert(agent_os_capability_lookup(&owner, attenuated, CAP_RIGHT_WRITE,
                                      &object, 0) == AGENT_OS_E_DENIED);
    /* Before transfer, the same numeric handle has no meaning in the empty
     * receiver namespace. */
    assert(agent_os_capability_lookup(&receiver, source, 0, &object, 0) !=
           AGENT_OS_OK);
    assert(agent_os_capability_transfer(&owner, source, &receiver,
                                       CAP_RIGHT_READ, &copied) == AGENT_OS_OK);
    assert(agent_os_capability_lookup(&receiver, copied, CAP_RIGHT_READ,
                                      &object, 0) == AGENT_OS_OK);
    assert(agent_os_capability_revoke(&owner, source) == AGENT_OS_OK);
    assert(agent_os_capability_lookup(&owner, source, 0, &object, 0) ==
           AGENT_OS_E_REVOKED);
    CapabilityHandle replacement;
    assert(agent_os_capability_mint(&owner, 0x5678, CAP_RIGHT_READ,
                                    &replacement) == AGENT_OS_OK);
    assert(replacement != source);
    assert(agent_os_capability_lookup(&owner, source, 0, &object, 0) ==
           AGENT_OS_E_REVOKED);

    AgentOsIpcEndpoint endpoint;
    IpcMessage received;
    assert(agent_os_ipc_send(0, 0) == AGENT_OS_E_INVAL);
    agent_os_ipc_endpoint_init(&endpoint);
    IpcMessage item = message(7);
    assert(agent_os_ipc_recv(&endpoint, &received) == AGENT_OS_E_TIMEOUT);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_OK);
    assert(agent_os_ipc_pending(&endpoint) == 1);
    assert(agent_os_ipc_recv(&endpoint, &received) == AGENT_OS_OK);
    assert(received.opcode == 7 && received.sequence == 1);
    item.header.version = 99;
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_INVAL);
    item = message(8);
    item.length = sizeof(item.words) + 1u;
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_INVAL);
    item = message(8);
    item.capability_count = AGENT_OS_IPC_MAX_CAPS + 1u;
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_INVAL);
    item = message(8);
    for (uint32_t index = 0; index < AGENT_OS_IPC_QUEUE_CAPACITY; ++index) {
        assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_OK);
    }
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_BUSY);
    assert(agent_os_ipc_endpoint_close(&endpoint) == AGENT_OS_OK);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_CLOSED);
    while (agent_os_ipc_recv(&endpoint, &received) == AGENT_OS_OK) {
    }
    assert(agent_os_ipc_recv(&endpoint, &received) == AGENT_OS_E_CLOSED);
    puts("G3 capability and IPC contracts OK");
    return 0;
}

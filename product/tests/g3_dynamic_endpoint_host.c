#include <assert.h>
#include <stdio.h>

#include "ipc_endpoint.h"

static IpcMessage message(uint32_t opcode) {
    IpcMessage value = {0};
    value.header.version = AGENT_OS_ABI_VERSION;
    value.header.size = sizeof(value);
    value.opcode = opcode;
    value.length = 1;
    value.words[0] = 0xD1;
    return value;
}

int main(void) {
    AgentOsIpcEndpoint endpoint = {0};
    IpcMessage received;
    IpcMessage item = message(0xD2);

    assert(agent_os_ipc_endpoint_create(&endpoint) == AGENT_OS_OK);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_OK);
    assert(agent_os_ipc_recv(&endpoint, &received) == AGENT_OS_OK);
    assert(received.opcode == item.opcode && received.sequence == 1);
    assert(agent_os_ipc_endpoint_close(&endpoint) == AGENT_OS_OK);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_CLOSED);
    assert(agent_os_ipc_endpoint_destroy(&endpoint) == AGENT_OS_OK);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_E_INVAL);
    assert(agent_os_ipc_endpoint_destroy(&endpoint) == AGENT_OS_E_INVAL);

    /* A destroyed slot can be safely reinitialized; old users must retain
     * their own generation-tagged capability and are not revived by this. */
    assert(agent_os_ipc_endpoint_create(&endpoint) == AGENT_OS_OK);
    assert(agent_os_ipc_send(&endpoint, &item) == AGENT_OS_OK);
    printf("dynamic endpoint create/close/destroy/reuse: PASS\n");
    return 0;
}

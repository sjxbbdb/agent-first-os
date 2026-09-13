#ifndef AGENT_OS_IPC_ENDPOINT_H
#define AGENT_OS_IPC_ENDPOINT_H

#include <stdint.h>

#include "ipc.h"

/* Bounded queue for the G3 synchronous IPC reference path. */
#define AGENT_OS_IPC_QUEUE_CAPACITY 16u

typedef struct AgentOsIpcEndpoint {
    IpcMessage queue[AGENT_OS_IPC_QUEUE_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint64_t next_sequence;
    /* The first blocking receive is represented by one explicit waiter.  A
     * later endpoint implementation can replace this with a wait queue while
     * preserving the syscall contract. */
    uint64_t waiting_process;
    uint64_t waiting_user_buffer;
    uint8_t closed;
    uint8_t active;
    uint8_t waiting;
    uint8_t reserved[5];
} AgentOsIpcEndpoint;

void agent_os_ipc_endpoint_init(AgentOsIpcEndpoint *endpoint);
/* Initialize an endpoint object in a Ring-0-owned pool slot.  The caller
 * supplies storage; this deliberately does not allocate from user memory. */
AgentOsStatus agent_os_ipc_endpoint_create(AgentOsIpcEndpoint *endpoint);
AgentOsStatus agent_os_ipc_endpoint_close(AgentOsIpcEndpoint *endpoint);
AgentOsStatus agent_os_ipc_endpoint_destroy(AgentOsIpcEndpoint *endpoint);
/* Syscall adapters validate endpoint/message pointers before calling these. */
AgentOsStatus agent_os_ipc_send(AgentOsIpcEndpoint *endpoint,
                                const IpcMessage *message);
AgentOsStatus agent_os_ipc_recv(AgentOsIpcEndpoint *endpoint,
                                IpcMessage *out_message);
uint32_t agent_os_ipc_pending(const AgentOsIpcEndpoint *endpoint);

#endif

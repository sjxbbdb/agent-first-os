#include "ipc_endpoint.h"

static AgentOsStatus validate_message(const IpcMessage *message) {
    if (message == 0 || message->header.version != AGENT_OS_ABI_VERSION ||
        message->header.size < sizeof(IpcMessage) ||
        message->length > sizeof(message->words) ||
        message->capability_count > AGENT_OS_IPC_MAX_CAPS) {
        return AGENT_OS_E_INVAL;
    }
    return AGENT_OS_OK;
}

void agent_os_ipc_endpoint_init(AgentOsIpcEndpoint *endpoint) {
    if (endpoint == 0) {
        return;
    }
    endpoint->head = 0;
    endpoint->tail = 0;
    endpoint->count = 0;
    endpoint->next_sequence = 1;
    endpoint->waiting_process = 0;
    endpoint->waiting_user_buffer = 0;
    endpoint->waiting = 0;
    endpoint->closed = 0;
    endpoint->active = 1;
    for (uint32_t index = 0; index < AGENT_OS_IPC_QUEUE_CAPACITY; ++index) {
        endpoint->queue[index] = (IpcMessage){0};
    }
}

AgentOsStatus agent_os_ipc_endpoint_create(AgentOsIpcEndpoint *endpoint) {
    if (endpoint == 0) {
        return AGENT_OS_E_INVAL;
    }
    agent_os_ipc_endpoint_init(endpoint);
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_ipc_endpoint_close(AgentOsIpcEndpoint *endpoint) {
    if (endpoint == 0 || !endpoint->active) {
        return AGENT_OS_E_INVAL;
    }
    endpoint->closed = 1;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_ipc_endpoint_destroy(AgentOsIpcEndpoint *endpoint) {
    if (endpoint == 0 || !endpoint->active) {
        return AGENT_OS_E_INVAL;
    }
    endpoint->closed = 1;
    endpoint->active = 0;
    endpoint->head = 0;
    endpoint->tail = 0;
    endpoint->count = 0;
    endpoint->waiting = 0;
    endpoint->waiting_process = 0;
    endpoint->waiting_user_buffer = 0;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_ipc_send(AgentOsIpcEndpoint *endpoint,
                                const IpcMessage *message) {
    if (endpoint == 0 || !endpoint->active) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsStatus status = validate_message(message);
    if (status != AGENT_OS_OK) {
        return status;
    }
    if (endpoint->closed) {
        return AGENT_OS_E_CLOSED;
    }
    if (endpoint->count >= AGENT_OS_IPC_QUEUE_CAPACITY) {
        return AGENT_OS_E_BUSY;
    }
    IpcMessage copy = *message;
    if (copy.sequence == 0) {
        copy.sequence = endpoint->next_sequence;
    }
    if (copy.sequence >= endpoint->next_sequence) {
        endpoint->next_sequence = copy.sequence + 1;
        if (endpoint->next_sequence == 0) {
            endpoint->next_sequence = 1;
        }
    }
    endpoint->queue[endpoint->tail] = copy;
    endpoint->tail = (endpoint->tail + 1u) % AGENT_OS_IPC_QUEUE_CAPACITY;
    endpoint->count += 1;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_ipc_recv(AgentOsIpcEndpoint *endpoint,
                                IpcMessage *out_message) {
    if (endpoint == 0 || !endpoint->active || out_message == 0) {
        return AGENT_OS_E_INVAL;
    }
    if (endpoint->count == 0) {
        return endpoint->closed ? AGENT_OS_E_CLOSED : AGENT_OS_E_TIMEOUT;
    }
    *out_message = endpoint->queue[endpoint->head];
    endpoint->head = (endpoint->head + 1u) % AGENT_OS_IPC_QUEUE_CAPACITY;
    endpoint->count -= 1;
    return AGENT_OS_OK;
}

uint32_t agent_os_ipc_pending(const AgentOsIpcEndpoint *endpoint) {
    return endpoint == 0 || !endpoint->active ? 0 : endpoint->count;
}

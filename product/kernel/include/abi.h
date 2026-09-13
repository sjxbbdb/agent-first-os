#ifndef AGENT_OS_ABI_H
#define AGENT_OS_ABI_H

#include <stdint.h>

#define AGENT_OS_ABI_VERSION UINT16_C(1)

typedef enum AgentOsStatus {
    AGENT_OS_OK = 0,
    AGENT_OS_E_INVAL = 1,
    AGENT_OS_E_FAULT = 2,
    AGENT_OS_E_NO_MEMORY = 3,
    AGENT_OS_E_BAD_CAP = 4,
    AGENT_OS_E_REVOKED = 5,
    AGENT_OS_E_DENIED = 6,
    AGENT_OS_E_TIMEOUT = 7,
    AGENT_OS_E_CLOSED = 8,
    AGENT_OS_E_BUSY = 9,
    AGENT_OS_E_NOT_FOUND = 10,
} AgentOsStatus;

typedef struct AbiHeader {
    uint16_t version;
    uint16_t size;
    uint32_t flags;
} AbiHeader;

_Static_assert(sizeof(AbiHeader) == 8, "ABI header size changed");

#endif

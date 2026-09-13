#ifndef AGENT_OS_IPC_H
#define AGENT_OS_IPC_H

#include <stdint.h>
#include "abi.h"

#define AGENT_OS_IPC_INLINE_WORDS 4
#define AGENT_OS_IPC_MAX_CAPS 4

typedef uint64_t AgentOsCapability;

typedef struct IpcMessage {
    AbiHeader header;
    uint32_t opcode;
    uint32_t length;
    uint32_t capability_count;
    uint32_t reserved;
    uint64_t sequence;
    uint64_t words[AGENT_OS_IPC_INLINE_WORDS];
    AgentOsCapability capabilities[AGENT_OS_IPC_MAX_CAPS];
} IpcMessage;

_Static_assert(sizeof(IpcMessage) == 96, "IPC message ABI size changed");

#endif

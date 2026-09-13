#ifndef AGENT_OS_POLICY_TOKEN_H
#define AGENT_OS_POLICY_TOKEN_H

#include <stdint.h>

#include "abi.h"
#include "process.h"

#define AGENT_OS_POLICY_TOKEN_SLOTS 32u

typedef uint64_t AgentOsPolicyTokenHandle;

typedef struct AgentOsPolicyTokenEntry {
    uint64_t owner_process;
    uint64_t capability;
    uint64_t action_nonce;
    uint64_t expires_at;
    uint32_t generation;
    uint8_t active;
    uint8_t reserved[3];
} AgentOsPolicyTokenEntry;

typedef struct AgentOsPolicyTokenTable {
    AgentOsPolicyTokenEntry entries[AGENT_OS_POLICY_TOKEN_SLOTS];
} AgentOsPolicyTokenTable;

void agent_os_policy_token_table_init(AgentOsPolicyTokenTable *table);
AgentOsStatus agent_os_policy_token_mint(AgentOsPolicyTokenTable *table,
                                         AgentOsProcessId owner_process,
                                         CapabilityHandle capability,
                                         uint64_t action_nonce,
                                         uint64_t expires_at,
                                         AgentOsPolicyTokenHandle *out);
AgentOsStatus agent_os_policy_token_consume(AgentOsPolicyTokenTable *table,
                                            AgentOsPolicyTokenHandle token,
                                            AgentOsProcessId caller,
                                            CapabilityHandle capability,
                                            uint64_t action_nonce,
                                            uint64_t now);
AgentOsStatus agent_os_policy_token_revoke(AgentOsPolicyTokenTable *table,
                                           AgentOsPolicyTokenHandle token,
                                           AgentOsProcessId caller);

#endif

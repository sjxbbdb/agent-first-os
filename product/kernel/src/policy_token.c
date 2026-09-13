#include "policy_token.h"

static uint32_t token_slot(AgentOsPolicyTokenHandle token) {
    uint32_t raw = (uint32_t)token;
    return raw == 0 ? UINT32_MAX : raw - 1u;
}

static uint32_t token_generation(AgentOsPolicyTokenHandle token) {
    return (uint32_t)(token >> 32);
}

static AgentOsPolicyTokenHandle make_token(uint32_t slot, uint32_t generation) {
    return ((uint64_t)generation << 32) | ((uint64_t)slot + 1u);
}

void agent_os_policy_token_table_init(AgentOsPolicyTokenTable *table) {
    if (table == 0) return;
    for (uint32_t index = 0; index < AGENT_OS_POLICY_TOKEN_SLOTS; ++index) {
        table->entries[index] = (AgentOsPolicyTokenEntry){
            .generation = 1,
        };
    }
}

AgentOsStatus agent_os_policy_token_mint(AgentOsPolicyTokenTable *table,
                                         AgentOsProcessId owner_process,
                                         CapabilityHandle capability,
                                         uint64_t action_nonce,
                                         uint64_t expires_at,
                                         AgentOsPolicyTokenHandle *out) {
    if (table == 0 || owner_process == AGENT_OS_PROCESS_INVALID ||
        capability == 0 || action_nonce == 0 || out == 0) {
        return AGENT_OS_E_INVAL;
    }
    for (uint32_t index = 0; index < AGENT_OS_POLICY_TOKEN_SLOTS; ++index) {
        AgentOsPolicyTokenEntry *entry = &table->entries[index];
        if (entry->active) continue;
        if (entry->generation == 0) entry->generation = 1;
        entry->owner_process = owner_process;
        entry->capability = capability;
        entry->action_nonce = action_nonce;
        entry->expires_at = expires_at;
        entry->active = 1;
        *out = make_token(index, entry->generation);
        return AGENT_OS_OK;
    }
    return AGENT_OS_E_NO_MEMORY;
}

AgentOsStatus agent_os_policy_token_consume(AgentOsPolicyTokenTable *table,
                                            AgentOsPolicyTokenHandle token,
                                            AgentOsProcessId caller,
                                            CapabilityHandle capability,
                                            uint64_t action_nonce,
                                            uint64_t now) {
    if (table == 0 || token == 0 || caller == AGENT_OS_PROCESS_INVALID ||
        capability == 0 || action_nonce == 0) {
        return AGENT_OS_E_INVAL;
    }
    uint32_t slot = token_slot(token);
    uint32_t generation = token_generation(token);
    if (slot >= AGENT_OS_POLICY_TOKEN_SLOTS || generation == 0) {
        return AGENT_OS_E_BAD_CAP;
    }
    AgentOsPolicyTokenEntry *entry = &table->entries[slot];
    if (!entry->active || entry->generation != generation) {
        return AGENT_OS_E_REVOKED;
    }
    if (entry->owner_process != caller || entry->capability != capability ||
        entry->action_nonce != action_nonce) {
        return AGENT_OS_E_DENIED;
    }
    if (entry->expires_at != 0 && now >= entry->expires_at) {
        return AGENT_OS_E_TIMEOUT;
    }
    /* Mark consumed before returning. A retry, including a re-entrant one,
     * observes the generation change and fails closed. */
    entry->active = 0;
    entry->owner_process = 0;
    entry->capability = 0;
    entry->action_nonce = 0;
    entry->expires_at = 0;
    entry->generation = generation == UINT32_MAX ? 0 : generation + 1u;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_policy_token_revoke(AgentOsPolicyTokenTable *table,
                                           AgentOsPolicyTokenHandle token,
                                           AgentOsProcessId caller) {
    if (table == 0 || token == 0 || caller == AGENT_OS_PROCESS_INVALID) {
        return AGENT_OS_E_INVAL;
    }
    uint32_t slot = token_slot(token);
    uint32_t generation = token_generation(token);
    if (slot >= AGENT_OS_POLICY_TOKEN_SLOTS || generation == 0) {
        return AGENT_OS_E_BAD_CAP;
    }
    AgentOsPolicyTokenEntry *entry = &table->entries[slot];
    if (!entry->active || entry->generation != generation) {
        return AGENT_OS_E_REVOKED;
    }
    if (entry->owner_process != caller) return AGENT_OS_E_DENIED;
    entry->active = 0;
    entry->owner_process = 0;
    entry->capability = 0;
    entry->action_nonce = 0;
    entry->expires_at = 0;
    entry->generation = generation == UINT32_MAX ? 0 : generation + 1u;
    return AGENT_OS_OK;
}

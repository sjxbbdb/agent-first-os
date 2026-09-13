#include "capability_table.h"

static uint32_t handle_slot(CapabilityHandle handle) {
    uint32_t raw = (uint32_t)handle;
    return raw == 0 ? UINT32_MAX : raw - 1u;
}

static uint32_t handle_generation(CapabilityHandle handle) {
    return (uint32_t)(handle >> 32);
}

static CapabilityHandle make_handle(uint32_t slot, uint32_t generation) {
    return ((uint64_t)generation << 32) | ((uint64_t)slot + 1u);
}

static uint64_t next_table_incarnation = 1;
static AgentOsStatus validate_rights(uint64_t rights);

static AgentOsStatus validate_handle(const AgentOsCapabilityTable *table,
                                     CapabilityHandle handle,
                                     const AgentOsCapabilityEntry **out_entry,
                                     uint32_t *out_slot) {
    if (table == 0 || handle == 0 || handle_generation(handle) == 0) {
        return AGENT_OS_E_BAD_CAP;
    }
    uint32_t slot = handle_slot(handle);
    if (slot >= AGENT_OS_CAPABILITY_SLOTS) {
        return AGENT_OS_E_BAD_CAP;
    }
    const AgentOsCapabilityEntry *entry = &table->entries[slot];
    if (!entry->active || entry->generation != handle_generation(handle)) {
        return AGENT_OS_E_REVOKED;
    }

    /* Walk the stored lineage without recursion.  Tables are kernel-owned
     * stable storage, while the incarnation check makes table reuse fail
     * closed even if a slot and generation happen to be reused. */
    const AgentOsCapabilityEntry *current_entry = entry;
    uint32_t depth = 0;
    while (current_entry->parent_table != 0) {
        if (current_entry->depth == 0 ||
            current_entry->depth > AGENT_OS_CAPABILITY_MAX_DEPTH ||
            depth++ >= AGENT_OS_CAPABILITY_MAX_DEPTH) {
            return AGENT_OS_E_REVOKED;
        }
        const AgentOsCapabilityTable *parent_table =
            current_entry->parent_table;
        if (current_entry->parent_incarnation != parent_table->incarnation) {
            return AGENT_OS_E_REVOKED;
        }
        CapabilityHandle parent_handle = current_entry->parent_handle;
        uint32_t parent_slot = handle_slot(parent_handle);
        if (parent_handle == 0 || handle_generation(parent_handle) == 0 ||
            parent_slot >= AGENT_OS_CAPABILITY_SLOTS) {
            return AGENT_OS_E_REVOKED;
        }
        const AgentOsCapabilityEntry *parent_entry =
            &parent_table->entries[parent_slot];
        if (!parent_entry->active ||
            parent_entry->generation != handle_generation(parent_handle) ||
            parent_entry->depth + 1u != current_entry->depth) {
            return AGENT_OS_E_REVOKED;
        }
        current_entry = parent_entry;
    }
    if (out_entry != 0) {
        *out_entry = entry;
    }
    if (out_slot != 0) {
        *out_slot = slot;
    }
    return AGENT_OS_OK;
}

static AgentOsStatus validate_rights(uint64_t rights) {
    const uint64_t known = CAP_RIGHT_READ | CAP_RIGHT_WRITE |
                           CAP_RIGHT_EXEC | CAP_RIGHT_MAP |
                           CAP_RIGHT_SEND | CAP_RIGHT_RECV |
                           CAP_RIGHT_TRANSFER | CAP_RIGHT_REVOKE |
                           CAP_RIGHT_ADMIN;
    return (rights & ~known) == 0 ? AGENT_OS_OK : AGENT_OS_E_INVAL;
}

void agent_os_capability_table_init(AgentOsCapabilityTable *table) {
    if (table == 0) {
        return;
    }
    table->incarnation = next_table_incarnation++;
    if (table->incarnation == 0) {
        /* Zero is reserved for an uninitialized lineage reference. */
        table->incarnation = next_table_incarnation++;
    }
    for (uint32_t index = 0; index < AGENT_OS_CAPABILITY_SLOTS; ++index) {
        AgentOsCapabilityEntry *entry = &table->entries[index];
        entry->object = 0;
        entry->rights = 0;
        entry->generation = 1;
        entry->active = 0;
        entry->depth = 0;
        entry->reserved[0] = 0;
        entry->reserved[1] = 0;
        entry->parent_table = 0;
        entry->parent_incarnation = 0;
        entry->parent_handle = 0;
    }
}

void agent_os_capability_table_revoke_all(AgentOsCapabilityTable *table) {
    if (table == 0) return;
    for (uint32_t i = 0; i < AGENT_OS_CAPABILITY_SLOTS; ++i) {
        AgentOsCapabilityEntry *entry = &table->entries[i];
        if (!entry->active) continue;
        entry->active = 0;
        entry->object = 0;
        entry->rights = 0;
        entry->parent_table = 0;
        entry->parent_incarnation = 0;
        entry->parent_handle = 0;
        entry->depth = 0;
        entry->generation = entry->generation == UINT32_MAX ? 0 : entry->generation + 1u;
    }
}

static AgentOsStatus mint_with_parent(
    AgentOsCapabilityTable *table,
    uint64_t object,
    uint64_t rights,
    const AgentOsCapabilityTable *parent_table,
    CapabilityHandle parent_handle,
    uint8_t depth,
    CapabilityHandle *out_handle) {
    if (table == 0 || out_handle == 0 || object == 0 ||
        validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    if (parent_table != 0 && depth > AGENT_OS_CAPABILITY_MAX_DEPTH) {
        return AGENT_OS_E_INVAL;
    }
    for (uint32_t index = 0; index < AGENT_OS_CAPABILITY_SLOTS; ++index) {
        AgentOsCapabilityEntry *entry = &table->entries[index];
        if (entry->active || entry->generation == 0) {
            continue;
        }
        entry->object = object;
        entry->rights = rights;
        entry->active = 1;
        entry->depth = parent_table == 0 ? 0 : depth;
        entry->parent_table = parent_table;
        entry->parent_incarnation =
            parent_table == 0 ? 0 : parent_table->incarnation;
        entry->parent_handle = parent_table == 0 ? 0 : parent_handle;
        *out_handle = make_handle(index, entry->generation);
        return AGENT_OS_OK;
    }
    return AGENT_OS_E_NO_MEMORY;
}

AgentOsStatus agent_os_capability_mint(AgentOsCapabilityTable *table,
                                       uint64_t object,
                                       uint64_t rights,
                                       CapabilityHandle *out_handle) {
    if (table == 0 || out_handle == 0 || object == 0 ||
        validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    return mint_with_parent(table, object, rights, 0, 0, 0, out_handle);
}

AgentOsStatus agent_os_capability_lookup(const AgentOsCapabilityTable *table,
                                         CapabilityHandle handle,
                                         uint64_t required_rights,
                                         uint64_t *out_object,
                                         uint64_t *out_rights) {
    const AgentOsCapabilityEntry *entry;
    if (out_object == 0 || validate_rights(required_rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsStatus status = validate_handle(table, handle, &entry, 0);
    if (status != AGENT_OS_OK) {
        return status;
    }
    if ((entry->rights & required_rights) != required_rights) {
        return AGENT_OS_E_DENIED;
    }
    *out_object = entry->object;
    if (out_rights != 0) {
        *out_rights = entry->rights;
    }
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_capability_restrict(AgentOsCapabilityTable *table,
                                           CapabilityHandle source,
                                           uint64_t rights,
                                           CapabilityHandle *out_handle) {
    uint64_t object;
    uint64_t source_rights;
    if (out_handle == 0 || validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    const AgentOsCapabilityEntry *source_entry;
    AgentOsStatus status = validate_handle(table, source, &source_entry, 0);
    if (status != AGENT_OS_OK) {
        return status;
    }
    object = source_entry->object;
    source_rights = source_entry->rights;
    if ((rights & ~source_rights) != 0) {
        return AGENT_OS_E_DENIED;
    }
    return mint_with_parent(table, object, rights, table, source,
                            (uint8_t)(source_entry->depth + 1u), out_handle);
}

AgentOsStatus agent_os_capability_transfer(
    const AgentOsCapabilityTable *source_table,
    CapabilityHandle source,
    AgentOsCapabilityTable *destination_table,
    uint64_t rights,
    CapabilityHandle *out_handle) {
    uint64_t object;
    uint64_t source_rights;
    if (destination_table == 0 || out_handle == 0 ||
        validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    const AgentOsCapabilityEntry *source_entry;
    AgentOsStatus status = validate_handle(source_table, source, &source_entry,
                                            0);
    if (status != AGENT_OS_OK) {
        return status;
    }
    object = source_entry->object;
    source_rights = source_entry->rights;
    if ((source_rights & CAP_RIGHT_TRANSFER) == 0) {
        return AGENT_OS_E_DENIED;
    }
    if ((rights & ~source_rights) != 0) {
        return AGENT_OS_E_DENIED;
    }
    return mint_with_parent(destination_table, object, rights, source_table,
                            source, (uint8_t)(source_entry->depth + 1u),
                            out_handle);
}

AgentOsStatus agent_os_capability_revoke(AgentOsCapabilityTable *table,
                                         CapabilityHandle handle) {
    const AgentOsCapabilityEntry *entry;
    uint32_t slot;
    AgentOsStatus status = validate_handle(table, handle, &entry, &slot);
    if (status != AGENT_OS_OK) {
        return status;
    }
    if ((entry->rights & CAP_RIGHT_REVOKE) == 0) {
        return AGENT_OS_E_DENIED;
    }
    AgentOsCapabilityEntry *mutable_entry = &table->entries[slot];
    mutable_entry->active = 0;
    mutable_entry->object = 0;
    mutable_entry->rights = 0;
    mutable_entry->parent_table = 0;
    mutable_entry->parent_incarnation = 0;
    mutable_entry->parent_handle = 0;
    mutable_entry->depth = 0;
    /* Generation zero is invalid and permanently retires a wrapped slot. */
    if (mutable_entry->generation == UINT32_MAX) {
        mutable_entry->generation = 0;
    } else {
        mutable_entry->generation += 1u;
    }
    return AGENT_OS_OK;
}

uint32_t agent_os_capability_revoke_object(AgentOsCapabilityTable *table,
                                           uint64_t object) {
    if (table == 0 || object == 0) return 0;
    uint32_t retired = 0;
    for (uint32_t index = 0; index < AGENT_OS_CAPABILITY_SLOTS; ++index) {
        AgentOsCapabilityEntry *entry = &table->entries[index];
        if (!entry->active || entry->object != object) continue;
        entry->active = 0;
        entry->object = 0;
        entry->rights = 0;
        entry->parent_table = 0;
        entry->parent_incarnation = 0;
        entry->parent_handle = 0;
        entry->depth = 0;
        entry->generation = entry->generation == UINT32_MAX ? 0 : entry->generation + 1u;
        retired += 1;
    }
    return retired;
}

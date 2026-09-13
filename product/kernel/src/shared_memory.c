#include "shared_memory.h"

static uint32_t handle_slot(AgentOsSharedMemoryHandle handle) {
    uint32_t raw = (uint32_t)handle;
    return raw == 0 ? UINT32_MAX : raw - 1u;
}

static uint32_t handle_generation(AgentOsSharedMemoryHandle handle) {
    return (uint32_t)(handle >> 32);
}

static AgentOsSharedMemoryHandle make_handle(uint32_t slot,
                                             uint32_t generation) {
    return ((uint64_t)generation << 32) | ((uint64_t)slot + 1u);
}

static AgentOsStatus validate_rights(uint64_t rights) {
    const uint64_t known = CAP_RIGHT_READ | CAP_RIGHT_WRITE |
                           CAP_RIGHT_EXEC | CAP_RIGHT_MAP;
    if (rights == 0 || (rights & ~known) != 0) {
        return AGENT_OS_E_INVAL;
    }
    return AGENT_OS_OK;
}

static AgentOsStatus validate_range(uint64_t base, uint64_t length) {
    if (base == 0 || length == 0 ||
        (base % AGENT_OS_SHM_PAGE_SIZE) != 0 ||
        (length % AGENT_OS_SHM_PAGE_SIZE) != 0 ||
        base > UINT64_MAX - length) {
        return AGENT_OS_E_INVAL;
    }
    return AGENT_OS_OK;
}

static AgentOsStatus validate_extent(uint64_t offset, uint64_t length) {
    if (length == 0 || (offset % AGENT_OS_SHM_PAGE_SIZE) != 0 ||
        (length % AGENT_OS_SHM_PAGE_SIZE) != 0 ||
        offset > UINT64_MAX - length) {
        return AGENT_OS_E_INVAL;
    }
    return AGENT_OS_OK;
}

static AgentOsStatus lookup_entry(const AgentOsSharedMemoryTable *table,
                                  AgentOsSharedMemoryHandle handle,
                                  const AgentOsSharedMemoryEntry **out_entry,
                                  uint32_t *out_slot) {
    if (table == 0 || handle == 0 || handle_generation(handle) == 0) {
        return AGENT_OS_E_BAD_CAP;
    }
    uint32_t slot = handle_slot(handle);
    if (slot >= AGENT_OS_SHM_MAX_OBJECTS) {
        return AGENT_OS_E_BAD_CAP;
    }
    const AgentOsSharedMemoryEntry *entry = &table->entries[slot];
    if (!entry->active || entry->generation != handle_generation(handle)) {
        return AGENT_OS_E_REVOKED;
    }
    if (out_entry != 0) {
        *out_entry = entry;
    }
    if (out_slot != 0) {
        *out_slot = slot;
    }
    return AGENT_OS_OK;
}

void agent_os_shm_table_init(AgentOsSharedMemoryTable *table) {
    if (table == 0) {
        return;
    }
    for (uint32_t index = 0; index < AGENT_OS_SHM_MAX_OBJECTS; ++index) {
        AgentOsSharedMemoryEntry *entry = &table->entries[index];
        entry->base = 0;
        entry->length = 0;
        entry->rights = 0;
        entry->generation = 1;
        entry->active = 0;
        entry->reserved[0] = 0;
        entry->reserved[1] = 0;
        entry->reserved[2] = 0;
    }
}

AgentOsStatus agent_os_shm_create(AgentOsSharedMemoryTable *table,
                                  uint64_t base,
                                  uint64_t length,
                                  uint64_t rights,
                                  AgentOsSharedMemoryHandle *out_handle) {
    if (table == 0 || out_handle == 0 ||
        validate_range(base, length) != AGENT_OS_OK ||
        validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    for (uint32_t index = 0; index < AGENT_OS_SHM_MAX_OBJECTS; ++index) {
        AgentOsSharedMemoryEntry *entry = &table->entries[index];
        if (entry->active) {
            continue;
        }
        if (entry->generation == 0) {
            entry->generation = 1;
        }
        entry->base = base;
        entry->length = length;
        entry->rights = rights;
        entry->active = 1;
        *out_handle = make_handle(index, entry->generation);
        return AGENT_OS_OK;
    }
    return AGENT_OS_E_NO_MEMORY;
}

AgentOsStatus agent_os_shm_lookup(const AgentOsSharedMemoryTable *table,
                                  AgentOsSharedMemoryHandle handle,
                                  uint64_t required_rights,
                                  uint64_t *out_base,
                                  uint64_t *out_length,
                                  uint64_t *out_rights) {
    const AgentOsSharedMemoryEntry *entry;
    if (out_base == 0 || out_length == 0 ||
        (required_rights != 0 && validate_rights(required_rights) !=
                                  AGENT_OS_OK)) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsStatus status = lookup_entry(table, handle, &entry, 0);
    if (status != AGENT_OS_OK) {
        return status;
    }
    if ((entry->rights & required_rights) != required_rights) {
        return AGENT_OS_E_DENIED;
    }
    *out_base = entry->base;
    *out_length = entry->length;
    if (out_rights != 0) {
        *out_rights = entry->rights;
    }
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_shm_restrict(AgentOsSharedMemoryTable *table,
                                    AgentOsSharedMemoryHandle source,
                                    uint64_t offset,
                                    uint64_t length,
                                    uint64_t rights,
                                    AgentOsSharedMemoryHandle *out_handle) {
    const AgentOsSharedMemoryEntry *entry;
    uint64_t base;
    uint64_t source_end;
    if (out_handle == 0 || validate_extent(offset, length) != AGENT_OS_OK ||
        validate_rights(rights) != AGENT_OS_OK) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsStatus status = lookup_entry(table, source, &entry, 0);
    if (status != AGENT_OS_OK) {
        return status;
    }
    if ((rights & ~entry->rights) != 0 || offset > entry->length ||
        length > entry->length - offset) {
        return AGENT_OS_E_DENIED;
    }
    base = entry->base + offset;
    source_end = entry->base + entry->length;
    if (base < entry->base || base > source_end ||
        length > source_end - base) {
        return AGENT_OS_E_INVAL;
    }
    return agent_os_shm_create(table, base, length, rights, out_handle);
}

AgentOsStatus agent_os_shm_revoke(AgentOsSharedMemoryTable *table,
                                  AgentOsSharedMemoryHandle handle) {
    const AgentOsSharedMemoryEntry *entry;
    uint32_t slot;
    AgentOsStatus status = lookup_entry(table, handle, &entry, &slot);
    if (status != AGENT_OS_OK) {
        return status;
    }
    AgentOsSharedMemoryEntry *mutable_entry = &table->entries[slot];
    mutable_entry->active = 0;
    mutable_entry->base = 0;
    mutable_entry->length = 0;
    mutable_entry->rights = 0;
    if (mutable_entry->generation == UINT32_MAX) {
        mutable_entry->generation = 0;
    } else {
        mutable_entry->generation += 1u;
    }
    return AGENT_OS_OK;
}

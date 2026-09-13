#ifndef AGENT_OS_SHARED_MEMORY_H
#define AGENT_OS_SHARED_MEMORY_H

#include <stdint.h>

#include "abi.h"
#include "capability.h"

/* G3 metadata only: no page-table mapping is performed by this module. */
#define AGENT_OS_SHM_PAGE_SIZE UINT64_C(4096)
#define AGENT_OS_SHM_MAX_OBJECTS 32u

typedef uint64_t AgentOsSharedMemoryHandle;

typedef struct AgentOsSharedMemoryEntry {
    uint64_t base;
    uint64_t length;
    uint64_t rights;
    uint32_t generation;
    uint8_t active;
    uint8_t reserved[3];
} AgentOsSharedMemoryEntry;

typedef struct AgentOsSharedMemoryTable {
    AgentOsSharedMemoryEntry entries[AGENT_OS_SHM_MAX_OBJECTS];
} AgentOsSharedMemoryTable;

void agent_os_shm_table_init(AgentOsSharedMemoryTable *table);

/* base and length are physical/object coordinates and must be page aligned. */
AgentOsStatus agent_os_shm_create(AgentOsSharedMemoryTable *table,
                                  uint64_t base,
                                  uint64_t length,
                                  uint64_t rights,
                                  AgentOsSharedMemoryHandle *out_handle);

AgentOsStatus agent_os_shm_lookup(const AgentOsSharedMemoryTable *table,
                                  AgentOsSharedMemoryHandle handle,
                                  uint64_t required_rights,
                                  uint64_t *out_base,
                                  uint64_t *out_length,
                                  uint64_t *out_rights);

/* Derive a page-aligned subrange and attenuated rights. */
AgentOsStatus agent_os_shm_restrict(AgentOsSharedMemoryTable *table,
                                    AgentOsSharedMemoryHandle source,
                                    uint64_t offset,
                                    uint64_t length,
                                    uint64_t rights,
                                    AgentOsSharedMemoryHandle *out_handle);

AgentOsStatus agent_os_shm_revoke(AgentOsSharedMemoryTable *table,
                                  AgentOsSharedMemoryHandle handle);

#endif

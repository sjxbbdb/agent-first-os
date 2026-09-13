#ifndef AGENT_OS_CAPABILITY_TABLE_H
#define AGENT_OS_CAPABILITY_TABLE_H

#include <stdint.h>

#include "abi.h"
#include "capability.h"

/*
 * G3 reference implementation.  The table is deliberately bounded so it can
 * run before the kernel allocator exists.  A future kernel-backed table must
 * preserve the handle and rights semantics documented here.
 */
#define AGENT_OS_CAPABILITY_SLOTS 64u
#define AGENT_OS_CAPABILITY_MAX_DEPTH 64u

typedef struct AgentOsCapabilityEntry {
    uint64_t object;
    uint64_t rights;
    uint32_t generation;
    uint8_t active;
    uint8_t depth;
    uint8_t reserved[2];
    const struct AgentOsCapabilityTable *parent_table;
    uint64_t parent_incarnation;
    CapabilityHandle parent_handle;
} AgentOsCapabilityEntry;

typedef struct AgentOsCapabilityTable {
    uint64_t incarnation;
    AgentOsCapabilityEntry entries[AGENT_OS_CAPABILITY_SLOTS];
} AgentOsCapabilityTable;

void agent_os_capability_table_init(AgentOsCapabilityTable *table);
/* Revoke every live handle owned by a terminating process. */
void agent_os_capability_table_revoke_all(AgentOsCapabilityTable *table);

/*
 * These APIs are Ring-0 primitives.  A syscall adapter must validate every
 * user pointer before passing it here; this module never trusts user memory.
 */

AgentOsStatus agent_os_capability_mint(AgentOsCapabilityTable *table,
                                       uint64_t object,
                                       uint64_t rights,
                                       CapabilityHandle *out_handle);

AgentOsStatus agent_os_capability_lookup(const AgentOsCapabilityTable *table,
                                         CapabilityHandle handle,
                                         uint64_t required_rights,
                                         uint64_t *out_object,
                                         uint64_t *out_rights);

/* Derive an attenuated handle.  The source remains valid. */
AgentOsStatus agent_os_capability_restrict(AgentOsCapabilityTable *table,
                                           CapabilityHandle source,
                                           uint64_t rights,
                                           CapabilityHandle *out_handle);

/* Copy an attenuated capability into another table.  Source is retained. */
AgentOsStatus agent_os_capability_transfer(
    const AgentOsCapabilityTable *source_table,
    CapabilityHandle source,
    AgentOsCapabilityTable *destination_table,
    uint64_t rights,
    CapabilityHandle *out_handle);

AgentOsStatus agent_os_capability_revoke(AgentOsCapabilityTable *table,
                                         CapabilityHandle handle);

/* Retire every live handle naming an object, including transferred siblings. */
uint32_t agent_os_capability_revoke_object(AgentOsCapabilityTable *table,
                                           uint64_t object);

#endif

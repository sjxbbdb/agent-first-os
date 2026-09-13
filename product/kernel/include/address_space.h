#ifndef AGENT_OS_ADDRESS_SPACE_H
#define AGENT_OS_ADDRESS_SPACE_H

#include <stdint.h>

#include "abi.h"

/* x86_64 paging constants.  The lower half is reserved for Ring 3 and the
 * upper half is the future shared kernel window. */
#define AGENT_OS_PAGE_SIZE UINT64_C(4096)
#define AGENT_OS_USER_LIMIT UINT64_C(0x0000800000000000)
#define AGENT_OS_KERNEL_BASE UINT64_C(0xFFFFFFFF80000000)
#define AGENT_OS_VM_MAX_REGIONS 128u

enum AgentOsVmPageFlags {
    AGENT_OS_VM_READ = UINT64_C(1) << 0,
    AGENT_OS_VM_WRITE = UINT64_C(1) << 1,
    AGENT_OS_VM_EXEC = UINT64_C(1) << 2,
    AGENT_OS_VM_USER = UINT64_C(1) << 3,
};

typedef struct AgentOsVmAllocation {
    uint64_t physical;
    uint64_t *virtual_address;
} AgentOsVmAllocation;

typedef AgentOsStatus (*AgentOsVmPageAllocator)(void *context,
                                                AgentOsVmAllocation *out);

typedef struct AgentOsVmAllocator {
    AgentOsVmPageAllocator allocate;
    void *context;
} AgentOsVmAllocator;

typedef struct AgentOsVmRegion {
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t length;
    uint64_t flags;
} AgentOsVmRegion;

/* A process owns its PML4 root.  Page-table entries store physical addresses;
 * this reference implementation requires the allocator's physical pages to
 * be directly addressable by the kernel (the bootstrap identity map satisfies
 * that requirement). */
typedef struct AgentOsAddressSpace {
    uint64_t root_physical;
    uint64_t *root_virtual;
    AgentOsVmAllocator allocator;
    AgentOsVmRegion regions[AGENT_OS_VM_MAX_REGIONS];
    uint32_t region_count;
} AgentOsAddressSpace;

AgentOsStatus agent_os_address_space_init(AgentOsAddressSpace *space,
                                          AgentOsVmAllocator allocator,
                                          const AgentOsAddressSpace *kernel_template);

AgentOsStatus agent_os_address_space_map(AgentOsAddressSpace *space,
                                         uint64_t virtual_address,
                                         uint64_t physical_address,
                                         uint64_t length,
                                         uint64_t flags);

/* Check every leaf in a user range, including PTE U/S and W^X permissions. */
AgentOsStatus agent_os_address_space_user_range_ok(
    const AgentOsAddressSpace *space,
    uint64_t virtual_address,
    uint64_t length,
    int write,
    int execute);

AgentOsStatus agent_os_address_space_lookup(const AgentOsAddressSpace *space,
                                            uint64_t virtual_address,
                                            uint64_t *out_physical,
                                            uint64_t *out_flags);

AgentOsStatus agent_os_address_space_validate(const AgentOsAddressSpace *space);

#endif

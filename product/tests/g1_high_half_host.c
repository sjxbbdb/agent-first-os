#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "address_space.h"

typedef struct Arena { uint8_t *bytes; uint32_t pages; uint32_t next; } Arena;

static AgentOsStatus alloc_page(void *context, AgentOsVmAllocation *out) {
    Arena *arena = context;
    if (arena == 0 || out == 0 || arena->next >= arena->pages) {
        return AGENT_OS_E_NO_MEMORY;
    }
    uint8_t *page = arena->bytes + (size_t)arena->next++ * AGENT_OS_PAGE_SIZE;
    out->physical = (uint64_t)(uintptr_t)page;
    out->virtual_address = (uint64_t *)(uintptr_t)page;
    return AGENT_OS_OK;
}

int main(void) {
    static uint8_t storage[128 * 4096] __attribute__((aligned(4096)));
    Arena arena = {storage, 128, 0};
    AgentOsVmAllocator allocator = {alloc_page, &arena};
    AgentOsAddressSpace space;
    assert(agent_os_address_space_init(&space, allocator, 0) == AGENT_OS_OK);

    const uint64_t low = UINT64_C(0x00101000);
    const uint64_t high = AGENT_OS_KERNEL_BASE;
    const uint64_t flags = AGENT_OS_VM_READ | AGENT_OS_VM_EXEC;
    assert(agent_os_address_space_map(&space, low, low, 0x1000, flags) == AGENT_OS_OK);
    assert(agent_os_address_space_map(&space, high, low, 0x1000, flags) == AGENT_OS_OK);
    uint64_t low_physical, low_flags, high_physical, high_flags;
    assert(agent_os_address_space_lookup(&space, low, &low_physical, &low_flags) == AGENT_OS_OK);
    assert(agent_os_address_space_lookup(&space, high, &high_physical, &high_flags) == AGENT_OS_OK);
    assert(low_physical == high_physical && low_physical == low);
    assert(low_flags == high_flags && low_flags == flags);
    assert(agent_os_address_space_map(&space, high + 0x1000, low + 0x1000,
                                      0x1000, flags | AGENT_OS_VM_USER) == AGENT_OS_E_DENIED);
    puts("G1 high-half host alias OK: shared physical page, identical W^X, supervisor-only");
    return 0;
}

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "address_space.h"

typedef struct Arena { uint8_t *bytes; uint32_t pages; uint32_t next; } Arena;

static AgentOsStatus alloc_page(void *context, AgentOsVmAllocation *out) {
    Arena *arena = context;
    if (arena == 0 || out == 0 || arena->next >= arena->pages) return AGENT_OS_E_NO_MEMORY;
    uint8_t *page = arena->bytes + (size_t)arena->next++ * AGENT_OS_PAGE_SIZE;
    out->physical = (uint64_t)(uintptr_t)page;
    out->virtual_address = (uint64_t *)(uintptr_t)page;
    return AGENT_OS_OK;
}

int main(void) {
    static uint8_t storage[256 * 4096] __attribute__((aligned(4096)));
    Arena arena = {storage, 256, 0};
    AgentOsVmAllocator allocator = {alloc_page, &arena};
    AgentOsAddressSpace kernel, first, second;
    assert(agent_os_address_space_init(&kernel, allocator, 0) == AGENT_OS_OK);
    assert(agent_os_address_space_map(&kernel, AGENT_OS_KERNEL_BASE,
                                      0x200000, 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_EXEC) == AGENT_OS_OK);
    /* A process gets a distinct PML4 root while inheriting the supervisor map. */
    assert(agent_os_address_space_init(&first, allocator, &kernel) == AGENT_OS_OK);
    assert(agent_os_address_space_init(&second, allocator, &kernel) == AGENT_OS_OK);
    assert(first.root_physical != second.root_physical);
    assert(first.root_virtual[511] == second.root_virtual[511]);
    assert(first.root_virtual[511] == kernel.root_virtual[511]);
    assert(agent_os_address_space_map(&first, 0x400000, 0x300000, 0x2000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_EXEC |
                                      AGENT_OS_VM_USER) == AGENT_OS_OK);
    assert(agent_os_address_space_map(&first, 0x600000, 0x500000, 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_WRITE |
                                      AGENT_OS_VM_USER) == AGENT_OS_OK);
    assert(agent_os_address_space_map(&second, 0x400000, 0x900000, 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_EXEC |
                                      AGENT_OS_VM_USER) == AGENT_OS_OK);
    assert(agent_os_address_space_user_range_ok(&first, 0x400000, 0x2000, 0, 1) == AGENT_OS_OK);
    assert(agent_os_address_space_user_range_ok(&first, 0x600000, 0x1000, 1, 0) == AGENT_OS_OK);
    assert(agent_os_address_space_user_range_ok(&first, 0x400000, 0x1000, 1, 0) == AGENT_OS_E_FAULT);
    assert(agent_os_address_space_user_range_ok(&first, 0x600000, 0x1000, 0, 1) == AGENT_OS_E_FAULT);
    assert(agent_os_address_space_map(&first, UINT64_C(0xFFFF800000000000), 0xA00000, 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_USER) == AGENT_OS_E_DENIED);
    assert(agent_os_address_space_map(&first, UINT64_C(0x00007FFFFFFFF000), 0xA00000, 0x2000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_USER) == AGENT_OS_E_INVAL);
    assert(agent_os_address_space_map(&first, 0x700000, 0xB00000, 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_WRITE |
                                      AGENT_OS_VM_EXEC | AGENT_OS_VM_USER) == AGENT_OS_E_INVAL);
    assert(agent_os_address_space_map(&first, 0x710000,
                                      UINT64_C(0x0010000000000000), 0x1000,
                                      AGENT_OS_VM_READ | AGENT_OS_VM_USER) == AGENT_OS_E_INVAL);
    assert(agent_os_address_space_map(&first, UINT64_C(0xFFFF000000000000),
                                      0xC00000, 0x1000, AGENT_OS_VM_READ) == AGENT_OS_E_INVAL);
    uint64_t physical, flags;
    assert(agent_os_address_space_lookup(&first, 0x600000, &physical, &flags) == AGENT_OS_OK);
    assert(physical == 0x500000 && (flags & AGENT_OS_VM_USER) != 0);
    assert(agent_os_address_space_validate(&first) == AGENT_OS_OK);
    puts("G2 address spaces OK: distinct CR3 roots, shared kernel, user W^X and bounds");
    return 0;
}

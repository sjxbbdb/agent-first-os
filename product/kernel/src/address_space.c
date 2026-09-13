#include "address_space.h"

#include <stddef.h>

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_WRITABLE UINT64_C(0x002)
#define PTE_USER UINT64_C(0x004)
#define PTE_NX (UINT64_C(1) << 63)
#define PTE_ADDRESS UINT64_C(0x000FFFFFFFFFF000)
#define PHYSICAL_LIMIT UINT64_C(0x0010000000000000)

static int add_overflow(uint64_t left, uint64_t right, uint64_t *out) {
    if (left > UINT64_MAX - right) {
        return 1;
    }
    *out = left + right;
    return 0;
}

static int canonical(uint64_t address) {
    /* 48-bit virtual addresses, with sign extension in bits 63:48. */
    uint64_t upper = address >> 48;
    uint64_t sign = (address >> 47) & 1;
    return (sign == 0 && upper == 0) ||
           (sign != 0 && upper == UINT64_C(0xFFFF));
}

static int aligned(uint64_t value) {
    return (value & (AGENT_OS_PAGE_SIZE - 1)) == 0;
}

static uint64_t page_flags_to_pte(uint64_t flags) {
    uint64_t pte = PTE_PRESENT;
    if (flags & AGENT_OS_VM_WRITE) {
        pte |= PTE_WRITABLE;
    }
    if (flags & AGENT_OS_VM_USER) {
        pte |= PTE_USER;
    }
    if ((flags & AGENT_OS_VM_EXEC) == 0) {
        pte |= PTE_NX;
    }
    return pte;
}

static AgentOsStatus alloc_table(AgentOsAddressSpace *space,
                                 uint64_t **out_virtual,
                                 uint64_t *out_physical) {
    if (space == 0 || out_virtual == 0 || out_physical == 0 ||
        space->allocator.allocate == 0) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsVmAllocation allocation = {0};
    AgentOsStatus status = space->allocator.allocate(space->allocator.context,
                                                     &allocation);
    if (status != AGENT_OS_OK || allocation.virtual_address == 0 ||
        !aligned(allocation.physical) ||
        !aligned((uint64_t)(uintptr_t)allocation.virtual_address)) {
        return status == AGENT_OS_OK ? AGENT_OS_E_FAULT : status;
    }
    for (size_t index = 0; index < 512; ++index) {
        allocation.virtual_address[index] = 0;
    }
    *out_virtual = allocation.virtual_address;
    *out_physical = allocation.physical;
    return AGENT_OS_OK;
}

static AgentOsStatus child_table(AgentOsAddressSpace *space,
                                 uint64_t *table,
                                 uint16_t index,
                                 int user,
                                 uint64_t **out_child) {
    uint64_t entry = table[index];
    if (entry & PTE_PRESENT) {
        if (user && (entry & PTE_USER) == 0) {
            return AGENT_OS_E_DENIED;
        }
        *out_child = (uint64_t *)(uintptr_t)(entry & PTE_ADDRESS);
        return AGENT_OS_OK;
    }
    uint64_t physical;
    uint64_t *child;
    AgentOsStatus status = alloc_table(space, &child, &physical);
    if (status != AGENT_OS_OK) {
        return status;
    }
    table[index] = physical | PTE_PRESENT | PTE_WRITABLE |
                   (user ? PTE_USER : 0);
    *out_child = child;
    return AGENT_OS_OK;
}

static AgentOsStatus walk_to_pte(const AgentOsAddressSpace *space,
                                 uint64_t virtual_address,
                                 int user,
                                 uint64_t **out_pte) {
    if (space == 0 || space->root_virtual == 0 || out_pte == 0 ||
        !canonical(virtual_address)) {
        return AGENT_OS_E_INVAL;
    }
    uint64_t *pml4 = space->root_virtual;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint16_t i4 = (uint16_t)((virtual_address >> 39) & 0x1FF);
    uint16_t i3 = (uint16_t)((virtual_address >> 30) & 0x1FF);
    uint16_t i2 = (uint16_t)((virtual_address >> 21) & 0x1FF);
    uint16_t i1 = (uint16_t)((virtual_address >> 12) & 0x1FF);
    uint64_t entry;
    entry = pml4[i4];
    if ((entry & PTE_PRESENT) == 0) return AGENT_OS_E_NOT_FOUND;
    if (user && (entry & PTE_USER) == 0) return AGENT_OS_E_DENIED;
    pdpt = (uint64_t *)(uintptr_t)(entry & PTE_ADDRESS);
    entry = pdpt[i3];
    if ((entry & PTE_PRESENT) == 0) return AGENT_OS_E_NOT_FOUND;
    if (entry & (UINT64_C(1) << 7)) return AGENT_OS_E_INVAL;
    if (user && (entry & PTE_USER) == 0) return AGENT_OS_E_DENIED;
    pd = (uint64_t *)(uintptr_t)(entry & PTE_ADDRESS);
    entry = pd[i2];
    if ((entry & PTE_PRESENT) == 0) return AGENT_OS_E_NOT_FOUND;
    if (entry & (UINT64_C(1) << 7)) return AGENT_OS_E_INVAL;
    if (user && (entry & PTE_USER) == 0) return AGENT_OS_E_DENIED;
    pt = (uint64_t *)(uintptr_t)(entry & PTE_ADDRESS);
    *out_pte = &pt[i1];
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_address_space_init(AgentOsAddressSpace *space,
                                          AgentOsVmAllocator allocator,
                                          const AgentOsAddressSpace *kernel_template) {
    if (space == 0 || allocator.allocate == 0) return AGENT_OS_E_INVAL;
    uint64_t *root;
    uint64_t physical;
    AgentOsAddressSpace fresh = {0};
    fresh.allocator = allocator;
    AgentOsStatus status = alloc_table(&fresh, &root, &physical);
    if (status != AGENT_OS_OK) return status;
    fresh.root_virtual = root;
    fresh.root_physical = physical;
    if (kernel_template != 0 && kernel_template->root_virtual != 0) {
        for (uint32_t index = 256; index < 512; ++index) {
            root[index] = kernel_template->root_virtual[index];
        }
    }
    *space = fresh;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_address_space_map(AgentOsAddressSpace *space,
                                         uint64_t virtual_address,
                                         uint64_t physical_address,
                                         uint64_t length,
                                         uint64_t flags) {
    uint64_t end;
    const uint64_t known = AGENT_OS_VM_READ | AGENT_OS_VM_WRITE |
                           AGENT_OS_VM_EXEC | AGENT_OS_VM_USER;
    if (space == 0 || space->root_virtual == 0 || length == 0 ||
        !aligned(virtual_address) || !aligned(physical_address) ||
        !aligned(length) || !canonical(virtual_address) ||
        add_overflow(virtual_address, length, &end) ||
        !canonical(end - 1) || physical_address > PTE_ADDRESS ||
        length > PHYSICAL_LIMIT - physical_address ||
        (flags & (AGENT_OS_VM_READ | AGENT_OS_VM_WRITE | AGENT_OS_VM_EXEC)) == 0 ||
        (flags & ~known) != 0 || (flags & (AGENT_OS_VM_WRITE | AGENT_OS_VM_EXEC)) ==
                                     (AGENT_OS_VM_WRITE | AGENT_OS_VM_EXEC)) {
        return AGENT_OS_E_INVAL;
    }
    if ((flags & AGENT_OS_VM_USER) &&
        (virtual_address >= AGENT_OS_USER_LIMIT || end > AGENT_OS_USER_LIMIT)) {
        return AGENT_OS_E_DENIED;
    }
    if (space->region_count >= AGENT_OS_VM_MAX_REGIONS) return AGENT_OS_E_NO_MEMORY;
    for (uint64_t offset = 0; offset < length; offset += AGENT_OS_PAGE_SIZE) {
        uint64_t va = virtual_address + offset;
        uint64_t pa = physical_address + offset;
        if (pa < physical_address) return AGENT_OS_E_INVAL;
        uint64_t *pdpt, *pd, *pt;
        int user = (flags & AGENT_OS_VM_USER) != 0;
        AgentOsStatus status = child_table(space, space->root_virtual,
                                           (uint16_t)((va >> 39) & 0x1FF), user,
                                           &pdpt);
        if (status != AGENT_OS_OK) return status;
        status = child_table(space, pdpt, (uint16_t)((va >> 30) & 0x1FF), user, &pd);
        if (status != AGENT_OS_OK) return status;
        status = child_table(space, pd, (uint16_t)((va >> 21) & 0x1FF), user, &pt);
        if (status != AGENT_OS_OK) return status;
        uint64_t *pte = &pt[(va >> 12) & 0x1FF];
        if (*pte & PTE_PRESENT) return AGENT_OS_E_BUSY;
        *pte = (pa & PTE_ADDRESS) | page_flags_to_pte(flags);
    }
    AgentOsVmRegion *region = &space->regions[space->region_count++];
    region->virtual_address = virtual_address;
    region->physical_address = physical_address;
    region->length = length;
    region->flags = flags;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_address_space_lookup(const AgentOsAddressSpace *space,
                                            uint64_t virtual_address,
                                            uint64_t *out_physical,
                                            uint64_t *out_flags) {
    if (out_physical == 0 || out_flags == 0 || !aligned(virtual_address))
        return AGENT_OS_E_INVAL;
    uint64_t *pte;
    AgentOsStatus status = walk_to_pte(space, virtual_address, 0, &pte);
    if (status != AGENT_OS_OK || (*pte & PTE_PRESENT) == 0) {
        return status == AGENT_OS_OK ? AGENT_OS_E_NOT_FOUND : status;
    }
    *out_physical = *pte & PTE_ADDRESS;
    *out_flags = AGENT_OS_VM_READ |
                 ((*pte & PTE_WRITABLE) ? AGENT_OS_VM_WRITE : 0) |
                 ((*pte & PTE_NX) ? 0 : AGENT_OS_VM_EXEC) |
                 ((*pte & PTE_USER) ? AGENT_OS_VM_USER : 0);
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_address_space_user_range_ok(
    const AgentOsAddressSpace *space, uint64_t virtual_address,
    uint64_t length, int write, int execute) {
    uint64_t end;
    if (length == 0 || add_overflow(virtual_address, length, &end) ||
        virtual_address >= AGENT_OS_USER_LIMIT || end > AGENT_OS_USER_LIMIT)
        return AGENT_OS_E_FAULT;
    uint64_t cursor = virtual_address & ~(AGENT_OS_PAGE_SIZE - 1);
    while (cursor < end) {
        uint64_t *pte;
        AgentOsStatus status = walk_to_pte(space, cursor, 1, &pte);
        if (status != AGENT_OS_OK || (*pte & PTE_PRESENT) == 0 ||
            (*pte & PTE_USER) == 0 || (write && (*pte & PTE_WRITABLE) == 0) ||
            (execute && (*pte & PTE_NX) != 0)) {
            return AGENT_OS_E_FAULT;
        }
        cursor += AGENT_OS_PAGE_SIZE;
    }
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_address_space_validate(const AgentOsAddressSpace *space) {
    if (space == 0 || space->root_virtual == 0 ||
        !aligned(space->root_physical) || space->region_count > AGENT_OS_VM_MAX_REGIONS)
        return AGENT_OS_E_INVAL;
    for (uint32_t i = 0; i < space->region_count; ++i) {
        const AgentOsVmRegion *region = &space->regions[i];
        uint64_t end;
        if (region->length == 0 || !aligned(region->virtual_address) ||
            !aligned(region->physical_address) || !aligned(region->length) ||
            add_overflow(region->virtual_address, region->length, &end) ||
            !canonical(region->virtual_address) || !canonical(end - 1) ||
            (region->flags & (AGENT_OS_VM_READ | AGENT_OS_VM_WRITE |
                              AGENT_OS_VM_EXEC)) == 0 ||
            (region->flags & (AGENT_OS_VM_WRITE | AGENT_OS_VM_EXEC)) ==
                (AGENT_OS_VM_WRITE | AGENT_OS_VM_EXEC))
            return AGENT_OS_E_INVAL;
        if ((region->flags & AGENT_OS_VM_USER) &&
            (region->virtual_address >= AGENT_OS_USER_LIMIT ||
             region->length > AGENT_OS_USER_LIMIT - region->virtual_address))
            return AGENT_OS_E_DENIED;
    }
    return AGENT_OS_OK;
}

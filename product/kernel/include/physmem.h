#ifndef AGENT_OS_PHYSMEM_H
#define AGENT_OS_PHYSMEM_H

#include <stdint.h>
#include "boot_info.h"

typedef struct PhysAllocator {
    uint64_t next_page;
    uint64_t end_page;
    uint32_t range_count;
    uint32_t current_range;
    struct {
        uint64_t next_page;
        uint64_t end_page;
    } ranges[16];
} PhysAllocator;

int phys_allocator_init(PhysAllocator *allocator, const BootInfo *boot_info);
uint64_t phys_alloc_page(PhysAllocator *allocator);

#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "physmem.h"

int main(void) {
    static E820Entry map[] = {
        { .base = 0x00300000, .length = 0x1000, .type = 1 },
        { .base = 0x00400000, .length = 0x9000, .type = 1 },
    };
    BootInfo boot = {0};
    boot.memory_map_addr = (uint64_t)(uintptr_t)map;
    boot.memory_map_count = 2;
    boot.memory_map_entry_size = sizeof(map[0]);
    boot.kernel_phys_start = 0x00400000;
    boot.kernel_phys_end = 0x00403001;
    boot.initrd_phys_start = 0x00405001;
    boot.initrd_phys_end = 0x00406001;
    PhysAllocator allocator;
    assert(phys_allocator_init(&allocator, &boot) != 0);
    assert(allocator.range_count >= 2);
    assert(phys_alloc_page(&allocator) == 0x00300000);
    assert(phys_alloc_page(&allocator) == 0x00404000);
    assert(phys_alloc_page(&allocator) == 0x00407000);
    puts("G2 physical allocator OK: multi-region E820 and kernel/initrd reservations");
    return 0;
}

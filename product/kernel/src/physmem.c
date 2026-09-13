#include "physmem.h"
#include "vm.h"

#define PHYS_MAX_RANGES 16u
#define PHYS_MAX_RESERVED 8u
typedef struct PhysRange { uint64_t start; uint64_t end; } PhysRange;

static uint64_t up(uint64_t v) { return v > UINT64_MAX - 0xfff ? 0 : (v + 0xfff) & ~UINT64_C(0xfff); }
static void sync(PhysAllocator *a) {
    if (a->current_range < a->range_count) {
        a->next_page = a->ranges[a->current_range].next_page;
        a->end_page = a->ranges[a->current_range].end_page;
    } else { a->next_page = 0; a->end_page = 0; }
}
static void add(PhysAllocator *a, uint64_t s, uint64_t e) {
    if (s < e && a->range_count < PHYS_MAX_RANGES) {
        a->ranges[a->range_count].next_page = s;
        a->ranges[a->range_count].end_page = e;
        a->range_count++;
    }
}

int phys_allocator_init(PhysAllocator *a, const BootInfo *b) {
    if (a == 0) return 0;
    a->range_count = 0; a->current_range = 0; a->next_page = 0; a->end_page = 0;
    if (b == 0 || b->memory_map_addr == 0 || b->memory_map_entry_size < sizeof(E820Entry)) return 0;
    PhysRange reserved[PHYS_MAX_RESERVED]; uint32_t rc = 0;
    const uint64_t starts[] = {b->kernel_phys_start, b->staging_phys_start, b->initrd_phys_start};
    const uint64_t ends[] = {b->kernel_phys_end, b->staging_phys_end, b->initrd_phys_end};
    for (uint32_t i = 0; i < 3 && rc < PHYS_MAX_RESERVED; ++i)
        if (starts[i] < ends[i]) reserved[rc++] = (PhysRange){starts[i], ends[i]};
    uint64_t map_bytes = (uint64_t)b->memory_map_count * b->memory_map_entry_size;
    if (rc < PHYS_MAX_RESERVED && map_bytes <= UINT64_MAX - b->memory_map_addr)
        reserved[rc++] = (PhysRange){b->memory_map_addr, b->memory_map_addr + map_bytes};
    const uint8_t *bytes = (const uint8_t *)(uintptr_t)b->memory_map_addr;
    for (uint32_t i = 0; i < b->memory_map_count; ++i) {
        const E820Entry *e = (const E820Entry *)(bytes + (uint64_t)i * b->memory_map_entry_size);
        if (e->type != 1 || e->length == 0 || e->base > UINT64_MAX - e->length) continue;
        /* Keep the low firmware handoff scratch area reserved, but allow
         * later low pages: the bootstrap page tables identity-map them and
         * vm.c can therefore safely use them for page-table construction. */
        uint64_t minimum = UINT64_C(0x10000);
        uint64_t s = up(e->base < minimum ? minimum : e->base);
        uint64_t end = (e->base + e->length) & ~UINT64_C(0xfff);
        if (s == 0 || s >= end) continue;
        uint64_t cursor = s;
        while (cursor < end && a->range_count < PHYS_MAX_RANGES) {
            uint64_t next = end, resume = end;
            for (uint32_t r = 0; r < rc; ++r) {
                /* Reserve every page touched by the byte range: round the
                 * start down and the end up, otherwise an unaligned kernel
                 * or initrd tail could be handed out. */
                uint64_t rs = reserved[r].start & ~UINT64_C(0xfff);
                uint64_t re = up(reserved[r].end);
                if (re == 0) re = UINT64_MAX & ~UINT64_C(0xfff);
                if (re <= cursor || rs >= end) continue;
                if (rs < next) { next = rs; resume = re < end ? re : end; }
            }
            if (next > cursor) add(a, cursor, next < end ? next : end);
            if (next >= end) break;
            cursor = resume > cursor ? resume : cursor + 0x1000;
        }
    }
    sync(a); return a->range_count != 0;
}

uint64_t phys_alloc_page(PhysAllocator *a) {
    if (a == 0) return 0;
    while (a->current_range < a->range_count) {
        uint64_t p = a->ranges[a->current_range].next_page;
        if (p < a->ranges[a->current_range].end_page) {
            a->ranges[a->current_range].next_page = p + 0x1000; sync(a); return p;
        }
        a->current_range++; sync(a);
    }
    return 0;
}

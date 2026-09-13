#include "vm.h"

#include <stddef.h>

#define PTE_PRESENT UINT64_C(0x001)
#define PTE_WRITABLE UINT64_C(0x002)
#define PTE_USER UINT64_C(0x004)
#define PTE_NX (UINT64_C(1) << 63)

/* These tables live in the kernel image and are identity mapped during the
 * bootstrap window.  The allocator and high-half relocation will replace
 * them with per-address-space tables in G2. */
static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd[512] __attribute__((aligned(4096)));
static uint64_t pt[512] __attribute__((aligned(4096)));

/* Early process roots and their child tables are allocated from an identity
 * mapped, kernel-owned pool.  This is intentionally bounded: the next stage
 * will replace it with the physical page allocator once CR3 switching is
 * proven on real hardware/QEMU. */
#define VM_PROCESS_PAGE_POOL_PAGES 32u
static uint8_t process_page_pool[VM_PROCESS_PAGE_POOL_PAGES * 4096]
    __attribute__((aligned(4096)));
static uint32_t process_page_pool_next;
static PhysAllocator *vm_phys_allocator;
static int vm_page_tables_pmm;
static uint64_t vm_pmm_pages[128];
static uint32_t vm_pmm_page_count;

extern char __kernel_text_start[];
extern char __kernel_text_end[];
extern char __kernel_rodata_start[];
extern char __kernel_rodata_end[];
extern char __user_text_start[];
extern char __user_text_end[];
extern char __kernel_data_start[];
extern char __kernel_data_end[];
extern char __kernel_bss_start[];
extern char __kernel_bss_end[];

static inline void write_cr3(uint64_t value) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

static AgentOsStatus process_page_allocate(void *context,
                                           AgentOsVmAllocation *out) {
    (void)context;
    if (out == 0) {
        return AGENT_OS_E_NO_MEMORY;
    }
    if (vm_phys_allocator != 0) {
        uint64_t physical = phys_alloc_page(vm_phys_allocator);
        if (physical != 0 && physical < AGENT_OS_IDENTITY_MAP_END) {
            uint8_t *page = (uint8_t *)(uintptr_t)physical;
            for (size_t index = 0; index < 4096; ++index) page[index] = 0;
            out->physical = physical;
            out->virtual_address = (uint64_t *)(uintptr_t)physical;
            vm_page_tables_pmm = 1;
            if (vm_pmm_page_count < 128) vm_pmm_pages[vm_pmm_page_count++] = physical;
            return AGENT_OS_OK;
        }
    }
    if (process_page_pool_next >= VM_PROCESS_PAGE_POOL_PAGES) {
        return AGENT_OS_E_NO_MEMORY;
    }
    uint8_t *page = process_page_pool +
        (size_t)process_page_pool_next++ * 4096u;
    for (size_t index = 0; index < 4096; ++index) {
        page[index] = 0;
    }
    out->physical = (uint64_t)(uintptr_t)page;
    out->virtual_address = (uint64_t *)(uintptr_t)page;
    return AGENT_OS_OK;
}

void vm_set_phys_allocator(PhysAllocator *allocator) {
    vm_phys_allocator = allocator;
    vm_page_tables_pmm = 0;
    vm_pmm_page_count = 0;
}

int vm_page_tables_use_phys_allocator(void) {
    return vm_page_tables_pmm;
}

AgentOsStatus vm_map_phys_allocator_pages(AgentOsAddressSpace *space) {
    if (space == 0) return AGENT_OS_E_INVAL;
    /* Current BIOS bootstrap PMM pages are deliberately restricted below the
     * 1 MiB kernel link address. Map that bounded supervisor scratch window as
     * one region so page-table child pages allocated while constructing the
     * roots remain software-addressable after CR3 switches. */
    AgentOsStatus window = agent_os_address_space_map(
        space, UINT64_C(0x00010000), UINT64_C(0x00010000),
        UINT64_C(0x000F0000), AGENT_OS_VM_READ | AGENT_OS_VM_WRITE);
    if (window != AGENT_OS_OK && window != AGENT_OS_E_BUSY) return window;
    uint32_t mapped = 0;
    while (mapped < vm_pmm_page_count) {
        uint64_t page = vm_pmm_pages[mapped++];
        AgentOsStatus status = agent_os_address_space_map(
            space, page, page, AGENT_OS_PAGE_SIZE,
            AGENT_OS_VM_READ | AGENT_OS_VM_WRITE);
        if (status != AGENT_OS_OK && status != AGENT_OS_E_BUSY) return status;
    }
    return AGENT_OS_OK;
}

static inline void enable_nxe(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(UINT32_C(0xC0000080)));
    low |= UINT32_C(1) << 11;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(UINT32_C(0xC0000080)));
}

static uint64_t page_floor(uint64_t value) {
    return value & UINT64_C(0xFFFFFFFFFFFFF000);
}

uint64_t vm_high_half_address(uint64_t low_address) {
    if (low_address < AGENT_OS_KERNEL_LINK_BASE ||
        low_address >= AGENT_OS_IDENTITY_MAP_END) {
        return 0;
    }
    return AGENT_OS_HIGH_HALF_KERNEL_BASE +
           (low_address - AGENT_OS_KERNEL_LINK_BASE);
}

static AgentOsStatus map_high_half_region(AgentOsAddressSpace *space,
                                          uint64_t low_start,
                                          uint64_t low_end,
                                          uint64_t flags) {
    if (low_end <= low_start || (low_start & 0xFFF) != 0 ||
        (low_end & 0xFFF) != 0) {
        return AGENT_OS_E_INVAL;
    }
    uint64_t high_start = vm_high_half_address(low_start);
    if (high_start == 0 || vm_high_half_address(low_end - 1) == 0) {
        return AGENT_OS_E_INVAL;
    }
    return agent_os_address_space_map(space, high_start, low_start,
                                      low_end - low_start, flags);
}

void vm_init(void) {
    process_page_pool_next = 0;
    for (size_t i = 0; i < 512; ++i) {
        pml4[i] = 0;
        pdpt[i] = 0;
        pd[i] = 0;
        pt[i] = 0;
    }
    /* Upper-level U/S bits must be set for a user leaf to be reachable;
     * supervisor-only leaves below still remain protected by their own bit. */
    pml4[0] = (uint64_t)(uintptr_t)pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pdpt[0] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pd[0] = (uint64_t)(uintptr_t)pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

    const uint64_t text_start = page_floor((uint64_t)(uintptr_t)__kernel_text_start);
    const uint64_t text_end = page_floor((uint64_t)(uintptr_t)__kernel_text_end - 1) + 0x1000;
    const uint64_t ro_start = page_floor((uint64_t)(uintptr_t)__kernel_rodata_start);
    const uint64_t ro_end = page_floor((uint64_t)(uintptr_t)__kernel_rodata_end - 1) + 0x1000;
    const uint64_t user_start = page_floor((uint64_t)(uintptr_t)__user_text_start);
    const uint64_t user_end = page_floor((uint64_t)(uintptr_t)__user_text_end - 1) + 0x1000;

    for (uint32_t index = 0; index < 512; ++index) {
        uint64_t address = (uint64_t)index << 12;
        uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_NX;
        if (address == 0) {
            /* Keep the null page unmapped so accidental function/data
             * pointers fail closed instead of aliasing physical address 0. */
            flags = 0;
        } else if (address >= text_start && address < text_end) {
            flags = PTE_PRESENT;
        } else if (address >= ro_start && address < ro_end) {
            flags = PTE_PRESENT;
        } else if (address >= user_start && address < user_end) {
            flags = PTE_PRESENT | PTE_USER;
        } else if (address == UINT64_C(0x001FE000) ||
                   address == UINT64_C(0x001FC000) ||
                   address == UINT64_C(0x001FA000)) {
            /* Bootstrap processes use disjoint user stack pages. */
            flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX;
        }
        pt[index] = address | flags;
    }

    enable_nxe();
    write_cr3((uint64_t)(uintptr_t)pml4);
}

void vm_load_address_space(uint64_t root_physical) {
    write_cr3(root_physical);
}

void vm_restore_bootstrap_address_space(void) {
    write_cr3((uint64_t)(uintptr_t)pml4);
}

AgentOsStatus vm_process_address_space_init(AgentOsAddressSpace *space) {
    if (space == 0) {
        return AGENT_OS_E_INVAL;
    }
    AgentOsAddressSpace kernel_template = {
        .root_physical = (uint64_t)(uintptr_t)pml4,
        .root_virtual = pml4,
    };
    AgentOsVmAllocator allocator = {
        .allocate = process_page_allocate,
        .context = 0,
    };
    AgentOsStatus status = agent_os_address_space_init(
        space, allocator, &kernel_template);
    if (status != AGENT_OS_OK) {
        return status;
    }

    /* User mappings are installed first so upper-level U/S bits remain
     * reachable, while kernel leaves below them stay supervisor-only. */
    uint64_t user_text_start = page_floor((uint64_t)(uintptr_t)__user_text_start);
    uint64_t user_text_end = page_floor((uint64_t)(uintptr_t)__user_text_end - 1) + 0x1000;
    status = agent_os_address_space_map(
        space, user_text_start, user_text_start,
        user_text_end - user_text_start,
        AGENT_OS_VM_READ | AGENT_OS_VM_EXEC | AGENT_OS_VM_USER);
    if (status != AGENT_OS_OK) return status;
    status = agent_os_address_space_map(
        space, UINT64_C(0x001FE000), UINT64_C(0x001FE000), 0x1000,
        AGENT_OS_VM_READ | AGENT_OS_VM_WRITE | AGENT_OS_VM_USER);
    if (status != AGENT_OS_OK) return status;
    status = agent_os_address_space_map(
        space, UINT64_C(0x001FC000), UINT64_C(0x001FC000), 0x1000,
        AGENT_OS_VM_READ | AGENT_OS_VM_WRITE | AGENT_OS_VM_USER);
    if (status != AGENT_OS_OK) return status;
    status = agent_os_address_space_map(
        space, UINT64_C(0x001FA000), UINT64_C(0x001FA000), 0x1000,
        AGENT_OS_VM_READ | AGENT_OS_VM_WRITE | AGENT_OS_VM_USER);
    if (status != AGENT_OS_OK) return status;

    uint64_t text_start = page_floor((uint64_t)(uintptr_t)__kernel_text_start);
    uint64_t text_end = page_floor((uint64_t)(uintptr_t)__kernel_text_end - 1) + 0x1000;
    uint64_t ro_start = page_floor((uint64_t)(uintptr_t)__kernel_rodata_start);
    uint64_t ro_end = page_floor((uint64_t)(uintptr_t)__kernel_rodata_end - 1) + 0x1000;
    uint64_t data_start = page_floor((uint64_t)(uintptr_t)__kernel_data_start);
    uint64_t data_end = page_floor((uint64_t)(uintptr_t)__kernel_data_end - 1) + 0x1000;
    uint64_t bss_start = page_floor((uint64_t)(uintptr_t)__kernel_bss_start);
    uint64_t bss_end = page_floor((uint64_t)(uintptr_t)__kernel_bss_end - 1) + 0x1000;
    const struct {
        uint64_t start;
        uint64_t end;
        uint64_t flags;
    } kernel_regions[] = {
        {text_start, text_end, AGENT_OS_VM_READ | AGENT_OS_VM_EXEC},
        {ro_start, ro_end, AGENT_OS_VM_READ},
        {data_start, data_end, AGENT_OS_VM_READ | AGENT_OS_VM_WRITE},
        {bss_start, bss_end, AGENT_OS_VM_READ | AGENT_OS_VM_WRITE},
    };
    for (size_t index = 0; index < sizeof(kernel_regions) / sizeof(kernel_regions[0]); ++index) {
        if (kernel_regions[index].end <= kernel_regions[index].start) continue;
        status = agent_os_address_space_map(
            space, kernel_regions[index].start, kernel_regions[index].start,
            kernel_regions[index].end - kernel_regions[index].start,
            kernel_regions[index].flags);
        if (status != AGENT_OS_OK) return status;
    }
    status = vm_prepare_high_half_alias(space);
    if (status != AGENT_OS_OK) return status;
    /* The bootstrap identity map makes PMM-backed page-table pages directly
     * addressable. Expose those supervisor-only pages in each process root so
     * later CR3 switches can still let Ring 0 walk/update its own tables. */
    status = vm_map_phys_allocator_pages(space);
    if (status != AGENT_OS_OK) return status;
    return agent_os_address_space_validate(space);
}

AgentOsStatus vm_prepare_high_half_alias(AgentOsAddressSpace *space) {
    if (space == 0) return AGENT_OS_E_INVAL;
    const uint64_t text_start = page_floor((uint64_t)(uintptr_t)__kernel_text_start);
    const uint64_t text_end = page_floor((uint64_t)(uintptr_t)__kernel_text_end - 1) + 0x1000;
    const uint64_t ro_start = page_floor((uint64_t)(uintptr_t)__kernel_rodata_start);
    const uint64_t ro_end = page_floor((uint64_t)(uintptr_t)__kernel_rodata_end - 1) + 0x1000;
    const uint64_t data_start = page_floor((uint64_t)(uintptr_t)__kernel_data_start);
    const uint64_t data_end = page_floor((uint64_t)(uintptr_t)__kernel_data_end - 1) + 0x1000;
    const uint64_t bss_start = page_floor((uint64_t)(uintptr_t)__kernel_bss_start);
    const uint64_t bss_end = page_floor((uint64_t)(uintptr_t)__kernel_bss_end - 1) + 0x1000;
    const struct {
        uint64_t start;
        uint64_t end;
        uint64_t flags;
    } regions[] = {
        {text_start, text_end, AGENT_OS_VM_READ | AGENT_OS_VM_EXEC},
        {ro_start, ro_end, AGENT_OS_VM_READ},
        {data_start, data_end, AGENT_OS_VM_READ | AGENT_OS_VM_WRITE},
        {bss_start, bss_end, AGENT_OS_VM_READ | AGENT_OS_VM_WRITE},
    };
    for (size_t index = 0; index < sizeof(regions) / sizeof(regions[0]); ++index) {
        if (regions[index].end <= regions[index].start) continue;
        AgentOsStatus status = map_high_half_region(
            space, regions[index].start, regions[index].end,
            regions[index].flags);
        if (status != AGENT_OS_OK) return status;
        uint64_t physical;
        uint64_t mapped_flags;
        uint64_t high = vm_high_half_address(regions[index].start);
        if (agent_os_address_space_lookup(space, high, &physical,
                                          &mapped_flags) != AGENT_OS_OK ||
            physical != regions[index].start || mapped_flags != regions[index].flags) {
            return AGENT_OS_E_FAULT;
        }
    }
    return AGENT_OS_OK;
}

int vm_user_range_ok(uint64_t address, uint64_t length, int write) {
    if (length == 0 || address + length < address) {
        return 0;
    }
    const uint64_t end = address + length;
    for (uint64_t cursor = page_floor(address); cursor < end; cursor += 0x1000) {
        if (cursor >= UINT64_C(0x00200000)) {
            return 0;
        }
        const uint64_t entry = pt[cursor >> 12];
        if ((entry & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)) {
            return 0;
        }
        if (write && (entry & PTE_WRITABLE) == 0) {
            return 0;
        }
        if (cursor > UINT64_MAX - 0x1000) {
            break;
        }
    }
    return 1;
}

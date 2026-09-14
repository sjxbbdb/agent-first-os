#ifndef AGENT_OS_VM_H
#define AGENT_OS_VM_H

#include <stdint.h>

#include "address_space.h"
#include "physmem.h"

/* M3/M6 bootstrap layout: low identity map remains active while this target
 * is reserved for the later high-half relocation. */
#define AGENT_OS_IDENTITY_MAP_END UINT64_C(0x00200000)
#define AGENT_OS_KERNEL_LINK_BASE UINT64_C(0x00100000)
#define AGENT_OS_HIGH_HALF_KERNEL_BASE UINT64_C(0xFFFFFFFF80000000)

typedef struct VmBootstrapLayout {
    uint64_t identity_map_end;
    uint64_t high_half_kernel_base;
    uint32_t boot_flags;
} VmBootstrapLayout;

/* Replaces the loader's temporary map with the kernel-owned 4 KiB map. */
void vm_init(void);
int vm_user_range_ok(uint64_t address, uint64_t length, int write);

/* Build a fully kernel-backed process root from the same identity-mapped
 * image used by the bootstrap.  The caller owns the process record's CR3
 * value; this function does not switch address spaces. */
AgentOsStatus vm_process_address_space_init(AgentOsAddressSpace *space);
void vm_load_address_space(uint64_t root_physical);
void vm_set_phys_allocator(PhysAllocator *allocator);
int vm_page_tables_use_phys_allocator(void);
AgentOsStatus vm_map_phys_allocator_pages(AgentOsAddressSpace *space);

/* Map a bounded supervisor-only PCI MMIO window into the bootstrap root.
 * The mapping is page-granular and deliberately excludes user access. */
AgentOsStatus vm_map_mmio_identity(uint64_t physical_address, uint64_t length);
/* Return to the bootstrap root after a bounded transition probe. */
void vm_restore_bootstrap_address_space(void);

/* Prepare the migration contract without changing the currently executing
 * low identity-mapped RIP or stack: kernel image pages are also reachable at
 * their high-half virtual address, with the same physical page and W^X leaf
 * permissions. */
AgentOsStatus vm_prepare_high_half_alias(AgentOsAddressSpace *space);
uint64_t vm_high_half_address(uint64_t low_address);

#endif

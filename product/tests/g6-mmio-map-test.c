#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* Supply the linker symbols used by vm_init when the real kernel linker
 * script is absent.  Including vm.c makes this test inspect the actual
 * bootstrap page tables and call the actual mapping helper. */
char __kernel_text_start[4096];
char __kernel_text_end[4096];
char __kernel_rodata_start[4096];
char __kernel_rodata_end[4096];
char __user_text_start[4096];
char __user_text_end[4096];
char __kernel_data_start[4096];
char __kernel_data_end[4096];
char __kernel_bss_start[4096];
char __kernel_bss_end[4096];

#define VM_HOST_TEST 1
#include "../kernel/src/vm.c"

int main(void) {
    const uint64_t first_region = UINT64_C(0xFDE00000);
    const uint64_t second_region = UINT64_C(0xFE000000);
    const uint64_t pte_flags = PTE_PRESENT | PTE_WRITABLE | PTE_PWT |
                               PTE_PCD | PTE_NX;

    assert(vm_map_mmio_identity(UINT64_C(0xFDE00000), 0x1000) == AGENT_OS_E_FAULT);
    vm_init();
    /* A pre-existing low identity mapping must survive an MMIO map. */
    const uint64_t old_low_pte = pt[1];
    assert(vm_map_mmio_identity(first_region + 0x123, 0x1000) == AGENT_OS_OK);
    assert(pt[1] == old_low_pte);
    assert(mmio_region_selected && mmio_region_base == first_region);
    assert((pdpt[3] & (PTE_PRESENT | PTE_WRITABLE | PTE_USER)) ==
           (PTE_PRESENT | PTE_WRITABLE));
    assert(mmio_pd[(first_region >> 21) & 0x1ff] != 0);
    assert(mmio_pt[0] == (first_region | pte_flags));
    assert((mmio_pt[0] & PTE_USER) == 0);

    /* Reusing the selected region is safe; selecting another would alias the
     * one shared PT and is rejected without changing the existing leaf. */
    assert(vm_map_mmio_identity(first_region + 0x123, 0x2000) == AGENT_OS_OK);
    const uint64_t preserved = mmio_pt[0];
    assert(vm_map_mmio_identity(second_region, 0x1000) == AGENT_OS_E_BUSY);
    assert(mmio_pt[0] == preserved);

    assert(vm_map_mmio_identity(UINT64_MAX - 0x10, 0x20) == AGENT_OS_E_INVAL);
    assert(vm_map_mmio_identity(UINT64_C(0xFCFFFFFF), 2) == AGENT_OS_E_INVAL);
    assert(vm_map_mmio_identity(UINT64_C(0xFDFFFFFF), 2) == AGENT_OS_E_INVAL);
    assert(vm_map_mmio_identity(UINT64_C(0xFEFFFFF0), 0x20) == AGENT_OS_E_INVAL);
    assert(vm_map_mmio_identity(first_region, 0) == AGENT_OS_E_INVAL);
    puts("G6 MMIO map OK: bounded single-region identity map, uncached supervisor leaves, no CR3 switch");
    return 0;
}

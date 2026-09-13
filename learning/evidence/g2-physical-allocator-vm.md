# G2 PMM to page-table allocator evidence

`vm.c` now accepts the bounded `PhysAllocator` as its first page-table page
source. Because the current BIOS identity map only makes low physical memory
software-addressable, the allocator starts after the reserved handoff scratch
area and `vm_map_phys_allocator_pages` exposes a supervisor-only low scratch
window in every process root. If PMM initialization is absent or exhausted,
the existing bounded `process_page_pool` remains the explicit fallback.

The BIOS kernel emits `G2 page tables PMM-backed identity-safe` when at least
one page-table page came from PMM. `g2-kernel-test.sh` requires that marker and
still verifies distinct CR3 roots, Ring 3 execution, task switching and idle.

```text
G2 page tables PMM-backed identity-safe
G2 CR3 switch OK
USER RING3 OK
USER RING3 TASK2 OK
SCHEDULER idle - all tasks exited
```

The host allocator test remains:

```text
G2 physical allocator OK: multi-region E820 and kernel/initrd reservations
```

This is a bounded bootstrap integration, not complete PMM ownership: the
identity-safe scratch window is fixed, allocation is not concurrent, pages are
never reclaimed, UEFI memory maps are not integrated, and the page-table pool
still falls back when a usable identity-mapped PMM page is unavailable.

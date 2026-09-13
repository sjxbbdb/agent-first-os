# G2 address-space slice

`kernel/src/address_space.c` is the first kernel-owned per-process page-table
builder. It is deliberately independent of the BIOS loader's temporary map so
the same object can be used by the BIOS and UEFI paths.

## Invariants

- A process owns a distinct 4 KiB-aligned PML4 root (`root_physical`).
- When a kernel template is supplied, PML4 entries 256 through 511 are copied
  into the new root. They remain supervisor-only and are the future shared
  high-half window.
- User virtual addresses are below `0x0000800000000000`; user mappings set the
  U/S bit through all four levels. Supervisor mappings can be used for the
  transition window, but are never accepted by `user_range_ok`.
- `WRITE | EXEC` is rejected at map time and all non-executable leaves carry
  NX. `user_range_ok` walks every leaf, so a numeric range check cannot bypass
  the PTE permissions.
- Page-table entries contain physical addresses. The current bootstrap
  allocator contract requires those physical pages to be directly addressable
  by Ring 0 (the identity map provides this during early boot).

## Kernel integration sequence

1. Replace the temporary `vm.c` PML4 load in `kernel_entry` with an allocator
   backed by the physical-frame allocator. Keep the allocator's virtual alias
   available while page tables are being built.
2. Build a kernel template once: map kernel text `R|X`, read-only data `R`,
   writable data/stack `R|W`, all with `USER=0`; map those regions at the
   high-half base when relocation is enabled.
3. For each `AgentOsProcess`, call `agent_os_address_space_init` with the
   template and map ELF `PT_LOAD` segments as `USER|R|X` or `USER|R|W` based on
   `p_flags`. Reject overlapping segments before calling `map`.
4. Allocate a separate user stack region as `USER|R|W`; leave its guard page
   unmapped. Validate the initial `rsp` with `user_range_ok` before `iretq`.
5. The BIOS Ring 3 fixture now loads `root_physical` into CR3 at launch and on
   each cooperative frame switch, and `SYS_WRITE` validates its pointer
   against the current process root. The first fixture is loaded from an
   embedded, validated ELF64 `PT_LOAD`; dynamic file loading, general
   multi-segment images, kernel stack/context switching, and timer preemption
   remain later work.

The host test proves the page-table and permission invariants, while the BIOS
fixture proves two distinct roots can be loaded and used through a real Ring 3
switch. The current allocator is still a bounded identity-mapped bootstrap
pool; UEFI parity and a physical-frame-backed allocator remain open gates.

#ifndef AGENT_OS_BOOT_INFO_V2_H
#define AGENT_OS_BOOT_INFO_V2_H

#include <stddef.h>
#include <stdint.h>

#define AGENT_OS_BOOT_INFO_MAGIC_V2 UINT64_C(0x41474f53424f4f54)
#define AGENT_OS_BOOT_INFO_VERSION_V2 UINT16_C(2)

enum AgentOsBootLoader {
    AGENT_OS_BOOT_LOADER_BIOS = 1,
    AGENT_OS_BOOT_LOADER_UEFI = 2,
};

enum AgentOsBootFlags {
    AGENT_OS_BOOT_V2_FLAG_IDENTITY_MAP = UINT32_C(1 << 0),
    AGENT_OS_BOOT_V2_FLAG_E820_VALID = UINT32_C(1 << 1),
    AGENT_OS_BOOT_V2_FLAG_UEFI_MEMORY_MAP = UINT32_C(1 << 2),
    AGENT_OS_BOOT_V2_FLAG_FRAMEBUFFER = UINT32_C(1 << 3),
    AGENT_OS_BOOT_V2_FLAG_ACPI = UINT32_C(1 << 4),
    AGENT_OS_BOOT_V2_FLAG_INITRD = UINT32_C(1 << 5),
};

typedef struct __attribute__((packed)) AgentOsE820EntryV2 {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t extended_attributes;
} AgentOsE820EntryV2;

/*
 * Loader-owned hand-off.  The layout deliberately keeps the v1 physical
 * addresses stable while replacing the BIOS-only GDT pointer with an explicit
 * loader enum.  Ring 0 owns GDT/TSS/IDT after kernel_entry and must not retain
 * pointers into loader-private state.
 */
typedef struct __attribute__((packed)) AgentOsBootInfoV2 {
    uint64_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t flags;

    uint64_t memory_map_addr;
    uint32_t memory_map_count;
    uint32_t memory_map_entry_size;

    uint64_t acpi_rsdp;
    uint64_t framebuffer_addr;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t loader_type;

    uint64_t boot_drive;
    uint64_t kernel_phys_start;
    uint64_t kernel_phys_end;
    uint64_t staging_phys_start;
    uint64_t staging_phys_end;
    uint64_t initrd_phys_start;
    uint64_t initrd_phys_end;
} AgentOsBootInfoV2;

_Static_assert(sizeof(AgentOsE820EntryV2) == 24, "BootInfo v2 E820 entry changed");
_Static_assert(sizeof(AgentOsBootInfoV2) == 128, "BootInfo v2 size changed");
_Static_assert(offsetof(AgentOsBootInfoV2, loader_type) == 68,
               "BootInfo v2 loader offset changed");
_Static_assert(offsetof(AgentOsBootInfoV2, kernel_phys_start) == 80,
               "BootInfo v2 kernel offset changed");
_Static_assert(offsetof(AgentOsBootInfoV2, staging_phys_end) == 104,
               "BootInfo v2 staging offset changed");
_Static_assert(offsetof(AgentOsBootInfoV2, initrd_phys_start) == 112,
               "BootInfo v2 initrd start offset changed");
_Static_assert(offsetof(AgentOsBootInfoV2, initrd_phys_end) == 120,
               "BootInfo v2 initrd end offset changed");

#endif

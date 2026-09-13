#ifndef AGENT_OS_INITRD_H
#define AGENT_OS_INITRD_H

#include <stddef.h>
#include <stdint.h>
#include "abi.h"

/* A deliberately small, read-only initrd container.  The loader copies one
 * bounded manifest and a table of ELF service images into contiguous physical
 * memory; Ring 0 validates ranges, while Supervisor remains the owner of
 * dependency and lifecycle policy in Ring 3. */
#define AGENT_OS_INITRD_MAGIC UINT64_C(0x41474f53494e4954)
#define AGENT_OS_INITRD_VERSION UINT16_C(2)
#define AGENT_OS_INITRD_HEADER_SIZE UINT16_C(40)
#define AGENT_OS_INITRD_MAX_SIZE UINT32_C(0x00080000)
#define AGENT_OS_INITRD_FLAG_MANIFEST_JSON UINT32_C(1u << 0)
#define AGENT_OS_INITRD_FLAG_SERVICE_ELF UINT32_C(1u << 1)
#define AGENT_OS_INITRD_MAX_SERVICES UINT32_C(8)
#define AGENT_OS_INITRD_SERVICE_ENTRY_SIZE UINT32_C(16)

typedef struct __attribute__((packed)) AgentOsInitrdHeader {
    uint64_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint32_t manifest_offset;
    uint32_t manifest_size;
    uint32_t service_offset;
    uint32_t service_size;
    uint32_t service_count;
    uint32_t flags;
} AgentOsInitrdHeader;

_Static_assert(sizeof(AgentOsInitrdHeader) == AGENT_OS_INITRD_HEADER_SIZE,
               "initrd header layout changed");

/* Offsets are absolute from the beginning of the initrd.  service_offset
 * points at this table and service_size covers the table plus all images. */
typedef struct __attribute__((packed)) AgentOsInitrdServiceEntry {
    uint32_t image_offset;
    uint32_t image_size;
    uint32_t manifest_index;
    uint32_t reserved;
} AgentOsInitrdServiceEntry;

_Static_assert(sizeof(AgentOsInitrdServiceEntry) ==
                   AGENT_OS_INITRD_SERVICE_ENTRY_SIZE,
               "initrd service entry layout changed");

static inline int agent_os_initrd_range_ok(uint32_t offset, uint32_t length,
                                           uint32_t total) {
    return offset <= total && length <= total - offset;
}

/* Select one already validated service image without granting Ring 3 access
 * to the initrd container. This is a bounded table lookup, not a filesystem
 * or dynamic linker. */
AgentOsStatus agent_os_initrd_select_service(const uint8_t *base,
                                             uint32_t total_size,
                                             uint32_t service_index,
                                             const uint8_t **out_image,
                                             uint64_t *out_size);

#endif

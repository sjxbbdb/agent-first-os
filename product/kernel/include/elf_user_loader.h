#ifndef AGENT_OS_ELF_USER_LOADER_H
#define AGENT_OS_ELF_USER_LOADER_H

#include <stdint.h>

#include "abi.h"

#define AGENT_OS_ELF_MAX_SEGMENTS 16u
#define AGENT_OS_ELF_USER_LIMIT UINT64_C(0x0000800000000000)

enum AgentOsElfSegmentFlags {
    AGENT_OS_ELF_R = UINT32_C(4),
    AGENT_OS_ELF_W = UINT32_C(2),
    AGENT_OS_ELF_X = UINT32_C(1),
};

typedef struct __attribute__((packed)) AgentOsElf64Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_size;
    uint16_t program_header_count;
    uint16_t section_header_size;
    uint16_t section_header_count;
    uint16_t section_name_index;
} AgentOsElf64Header;

typedef struct __attribute__((packed)) AgentOsElf64ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} AgentOsElf64ProgramHeader;

typedef struct AgentOsElfLoadSegment {
    uint64_t virtual_address;
    uint64_t file_offset;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t flags;
} AgentOsElfLoadSegment;

typedef struct AgentOsElfLoadPlan {
    uint64_t entry;
    uint32_t segment_count;
    AgentOsElfLoadSegment segments[AGENT_OS_ELF_MAX_SEGMENTS];
} AgentOsElfLoadPlan;

AgentOsStatus agent_os_elf64_plan(const uint8_t *image,
                                  uint64_t image_size,
                                  AgentOsElfLoadPlan *out_plan);

/* Plan a bounded ET_EXEC or ET_DYN image. ET_DYN virtual addresses and entry
 * are relocated by the caller-supplied page-aligned user load bias. */
AgentOsStatus agent_os_elf64_plan_with_bias(const uint8_t *image,
                                            uint64_t image_size,
                                            uint64_t load_bias,
                                            AgentOsElfLoadPlan *out_plan);

AgentOsStatus agent_os_elf64_load(const uint8_t *image,
                                  uint64_t image_size,
                                  uint8_t *memory,
                                  uint64_t memory_base,
                                  uint64_t memory_size,
                                  AgentOsElfLoadPlan *out_plan);

#endif

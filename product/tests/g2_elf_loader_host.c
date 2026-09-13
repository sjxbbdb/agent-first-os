#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "elf_user_loader.h"

static uint64_t make_image(uint8_t *image, uint64_t capacity, uint32_t flags,
                           uint64_t entry, uint64_t file_size, uint64_t memory_size) {
    const uint64_t payload = 0x1000;
    if (capacity < payload || file_size > capacity - payload) return 0;
    memset(image, 0, (size_t)capacity);
    AgentOsElf64Header *header = (AgentOsElf64Header *)image;
    header->ident[0] = 0x7F; header->ident[1] = 'E'; header->ident[2] = 'L'; header->ident[3] = 'F';
    header->ident[4] = 2; header->ident[5] = 1; header->ident[6] = 1;
    header->type = 2; header->machine = 0x3E; header->version = 1; header->entry = entry;
    header->header_size = sizeof(*header); header->program_header_offset = sizeof(*header);
    header->program_header_size = sizeof(AgentOsElf64ProgramHeader); header->program_header_count = 1;
    AgentOsElf64ProgramHeader *ph = (AgentOsElf64ProgramHeader *)(image + sizeof(*header));
    ph->type = 1; ph->flags = flags; ph->offset = payload; ph->virtual_address = 0x400000;
    ph->file_size = file_size; ph->memory_size = memory_size; ph->alignment = 0x1000;
    for (uint64_t index = 0; index < file_size; ++index) image[payload + index] = (uint8_t)(index + 1);
    return payload + file_size;
}

int main(void) {
    uint8_t image[0x3000], memory[0x3000], untouched[0x3000];
    AgentOsElfLoadPlan plan;
    uint64_t image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x400000, 3, 0x1000);
    memset(memory, 0xA5, sizeof(memory));
    assert(agent_os_elf64_load(image, image_size, memory, 0x400000, sizeof(memory), &plan) == AGENT_OS_OK);
    assert(plan.segment_count == 1 && plan.entry == 0x400000);
    assert(memory[0] == 1 && memory[2] == 3 && memory[3] == 0 && memory[0xFFF] == 0);

    image[0] = 0;
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x500000, 3, 0x1000);
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_DENIED);
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_W | AGENT_OS_ELF_X, 0x400000, 3, 0x1000);
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x400000, 0x1001, 0x1000);
    assert(image_size == 0x2001);
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x400000, 3, 0);
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x400000, 3, 0x1000);
    ((AgentOsElf64Header *)image)->type = 3;
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);
    ((AgentOsElf64Header *)image)->type = 2;
    ((AgentOsElf64ProgramHeader *)(image + sizeof(AgentOsElf64Header)))->virtual_address = 0x400800;
    assert(agent_os_elf64_plan(image, image_size, &plan) == AGENT_OS_E_INVAL);

    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X, 0x400000, 3, 0x1000);
    memset(memory, 0x5A, sizeof(memory));
    memcpy(untouched, memory, sizeof(memory));
    assert(agent_os_elf64_load(image, image_size, memory, 0x400000, 2, &plan) == AGENT_OS_E_FAULT);
    assert(memcmp(memory, untouched, sizeof(memory)) == 0);

    /* PIE/ET_DYN is relative: the loader must apply a checked page-aligned
     * bias to both the entry and every PT_LOAD virtual address. */
    image_size = make_image(image, sizeof(image), AGENT_OS_ELF_R | AGENT_OS_ELF_X,
                            0, 3, 0x1000);
    ((AgentOsElf64Header *)image)->type = 3;
    ((AgentOsElf64ProgramHeader *)(image + sizeof(AgentOsElf64Header)))->virtual_address = 0;
    assert(agent_os_elf64_plan_with_bias(image, image_size, 0x400000, &plan) == AGENT_OS_OK);
    assert(plan.entry == 0x400000 && plan.segment_count == 1 &&
           plan.segments[0].virtual_address == 0x400000);
    assert(agent_os_elf64_plan_with_bias(image, image_size, 0x400001, &plan) == AGENT_OS_E_INVAL);
    assert(agent_os_elf64_plan_with_bias(image, image_size, AGENT_OS_ELF_USER_LIMIT,
                                         &plan) != AGENT_OS_OK);
    puts("G2 ELF user loader OK: ET_EXEC bounds, W^X, page policy, zeroing and atomic destination validation");
    return 0;
}

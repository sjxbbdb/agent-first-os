#include "elf_user_loader.h"

#define ELF_ET_EXEC UINT16_C(2)
#define ELF_PT_NULL UINT32_C(0)
#define ELF_PT_LOAD UINT32_C(1)
#define ELF_CLASS_64 UINT8_C(2)
#define ELF_DATA_LSB UINT8_C(1)
#define ELF_VERSION_CURRENT UINT8_C(1)
#define ELF_MACHINE_X86_64 UINT16_C(0x3E)
#define ELF_PAGE_SIZE UINT64_C(0x1000)

static int add_overflow(uint64_t left, uint64_t right, uint64_t *out) {
    if (left > UINT64_MAX - right) return 1;
    *out = left + right;
    return 0;
}
static int mul_overflow(uint64_t left, uint64_t right, uint64_t *out) {
    if (left != 0 && right > UINT64_MAX / left) return 1;
    *out = left * right;
    return 0;
}
static int range_within(uint64_t start, uint64_t length,
                        uint64_t limit_start, uint64_t limit_size) {
    uint64_t end, limit_end;
    if (add_overflow(start, length, &end) ||
        add_overflow(limit_start, limit_size, &limit_end)) return 0;
    return start >= limit_start && end <= limit_end;
}
static int ranges_overlap(uint64_t left, uint64_t left_size,
                          uint64_t right, uint64_t right_size) {
    uint64_t left_end, right_end;
    if (add_overflow(left, left_size, &left_end) ||
        add_overflow(right, right_size, &right_end)) return 1;
    return left < right_end && right < left_end;
}
static int power_of_two(uint64_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}
static int round_up_page(uint64_t value, uint64_t *out) {
    uint64_t adjusted;
    if (add_overflow(value, ELF_PAGE_SIZE - 1, &adjusted)) return 1;
    *out = adjusted & ~(ELF_PAGE_SIZE - 1);
    return 0;
}

static AgentOsStatus plan_with_bias_internal(const uint8_t *image,
                                             uint64_t image_size,
                                             uint64_t load_bias,
                                             AgentOsElfLoadPlan *out_plan) {
    if (image == 0 || out_plan == 0 || image_size < sizeof(AgentOsElf64Header)) return AGENT_OS_E_INVAL;
    const AgentOsElf64Header *header = (const AgentOsElf64Header *)image;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' || header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != ELF_CLASS_64 || header->ident[5] != ELF_DATA_LSB || header->ident[6] != ELF_VERSION_CURRENT || header->ident[7] != 0 ||
        (header->type != ELF_ET_EXEC && header->type != UINT16_C(3)) ||
        (header->type == ELF_ET_EXEC && load_bias != 0) ||
        (header->type == UINT16_C(3) && ((load_bias & (ELF_PAGE_SIZE - 1)) != 0 || load_bias == 0)) ||
        header->machine != ELF_MACHINE_X86_64 || header->version != 1 || header->flags != 0 ||
        header->header_size != sizeof(AgentOsElf64Header) || header->program_header_size != sizeof(AgentOsElf64ProgramHeader) ||
        header->program_header_count == 0 || header->program_header_count > 64) return AGENT_OS_E_INVAL;
    uint64_t ph_bytes;
    if (mul_overflow(header->program_header_size, header->program_header_count, &ph_bytes) || !range_within(header->program_header_offset, ph_bytes, 0, image_size)) return AGENT_OS_E_INVAL;
    AgentOsElfLoadPlan plan = {0};
    if (add_overflow(header->entry, load_bias, &plan.entry) ||
        plan.entry >= AGENT_OS_ELF_USER_LIMIT) return AGENT_OS_E_INVAL;
    for (uint16_t index = 0; index < header->program_header_count; ++index) {
        uint64_t index_bytes, offset;
        if (mul_overflow(index, header->program_header_size, &index_bytes) || add_overflow(header->program_header_offset, index_bytes, &offset)) return AGENT_OS_E_INVAL;
        const AgentOsElf64ProgramHeader *ph = (const AgentOsElf64ProgramHeader *)(image + offset);
        if (ph->type == ELF_PT_NULL) continue;
        if (ph->type != ELF_PT_LOAD || ph->memory_size == 0 || plan.segment_count >= AGENT_OS_ELF_MAX_SEGMENTS || ph->file_size > ph->memory_size ||
            !range_within(ph->offset, ph->file_size, 0, image_size) || ph->virtual_address >= AGENT_OS_ELF_USER_LIMIT ||
            (ph->virtual_address & (ELF_PAGE_SIZE - 1)) != 0 || ph->memory_size > AGENT_OS_ELF_USER_LIMIT - ph->virtual_address ||
            (ph->flags & ~(AGENT_OS_ELF_R | AGENT_OS_ELF_W | AGENT_OS_ELF_X)) != 0 || (ph->flags & (AGENT_OS_ELF_W | AGENT_OS_ELF_X)) == (AGENT_OS_ELF_W | AGENT_OS_ELF_X) ||
            (ph->flags & (AGENT_OS_ELF_R | AGENT_OS_ELF_W | AGENT_OS_ELF_X)) == 0 || (ph->offset & (ELF_PAGE_SIZE - 1)) != 0 ||
            (ph->alignment > 1 && (!power_of_two(ph->alignment) || (ph->virtual_address % ph->alignment) != (ph->offset % ph->alignment)))) return AGENT_OS_E_INVAL;
        uint64_t segment_virtual, segment_end, mapped_end;
        if (add_overflow(ph->virtual_address, load_bias, &segment_virtual) ||
            add_overflow(segment_virtual, ph->memory_size, &segment_end) ||
            segment_virtual >= AGENT_OS_ELF_USER_LIMIT ||
            round_up_page(segment_end, &mapped_end) || mapped_end > AGENT_OS_ELF_USER_LIMIT) return AGENT_OS_E_INVAL;
        for (uint32_t prior = 0; prior < plan.segment_count; ++prior) {
            uint64_t prior_end = plan.segments[prior].virtual_address + plan.segments[prior].memory_size, prior_mapped_end;
            if (segment_virtual < prior_end && plan.segments[prior].virtual_address < segment_end) return AGENT_OS_E_BUSY;
            if (round_up_page(prior_end, &prior_mapped_end) == 0 && segment_virtual < prior_mapped_end && plan.segments[prior].virtual_address < mapped_end) return AGENT_OS_E_BUSY;
        }
        plan.segments[plan.segment_count++] = (AgentOsElfLoadSegment){.virtual_address = segment_virtual, .file_offset = ph->offset, .file_size = ph->file_size, .memory_size = ph->memory_size, .flags = ph->flags};
    }
    if (plan.segment_count == 0) return AGENT_OS_E_INVAL;
    int entry_executable = 0;
    for (uint32_t index = 0; index < plan.segment_count; ++index) {
        const AgentOsElfLoadSegment *segment = &plan.segments[index];
        uint64_t file_end = segment->virtual_address + segment->file_size;
        if ((segment->flags & AGENT_OS_ELF_X) != 0 && plan.entry >= segment->virtual_address && plan.entry < file_end) entry_executable = 1;
    }
    if (!entry_executable) return AGENT_OS_E_DENIED;
    *out_plan = plan;
    return AGENT_OS_OK;
}

AgentOsStatus agent_os_elf64_plan_with_bias(const uint8_t *image,
                                            uint64_t image_size,
                                            uint64_t load_bias,
                                            AgentOsElfLoadPlan *out_plan) {
    return plan_with_bias_internal(image, image_size, load_bias, out_plan);
}

AgentOsStatus agent_os_elf64_plan(const uint8_t *image, uint64_t image_size,
                                  AgentOsElfLoadPlan *out_plan) {
    return plan_with_bias_internal(image, image_size, 0, out_plan);
}

AgentOsStatus agent_os_elf64_load(const uint8_t *image, uint64_t image_size, uint8_t *memory, uint64_t memory_base, uint64_t memory_size, AgentOsElfLoadPlan *out_plan) {
    AgentOsElfLoadPlan plan;
    AgentOsStatus status = agent_os_elf64_plan(image, image_size, &plan);
    if (status != AGENT_OS_OK || memory == 0) return status == AGENT_OS_OK ? AGENT_OS_E_INVAL : status;
    if (ranges_overlap((uint64_t)(uintptr_t)image, image_size, (uint64_t)(uintptr_t)memory, memory_size)) return AGENT_OS_E_FAULT;
    for (uint32_t index = 0; index < plan.segment_count; ++index) {
        const AgentOsElfLoadSegment *segment = &plan.segments[index];
        uint64_t mapped_end;
        if (round_up_page(segment->virtual_address + segment->memory_size, &mapped_end) || mapped_end < segment->virtual_address || !range_within(segment->virtual_address, mapped_end - segment->virtual_address, memory_base, memory_size)) return AGENT_OS_E_FAULT;
    }
    for (uint32_t index = 0; index < plan.segment_count; ++index) {
        const AgentOsElfLoadSegment *segment = &plan.segments[index];
        uint64_t mapped_end;
        (void)round_up_page(segment->virtual_address + segment->memory_size, &mapped_end);
        uint64_t destination_offset = segment->virtual_address - memory_base, mapped_size = mapped_end - segment->virtual_address;
        for (uint64_t byte = 0; byte < mapped_size; ++byte) memory[destination_offset + byte] = byte < segment->file_size ? image[segment->file_offset + byte] : 0;
    }
    if (out_plan != 0) *out_plan = plan;
    return AGENT_OS_OK;
}

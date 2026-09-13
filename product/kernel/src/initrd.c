#include "initrd.h"

AgentOsStatus agent_os_initrd_select_service(const uint8_t *base,
                                             uint32_t total_size,
                                             uint32_t service_index,
                                             const uint8_t **out_image,
                                             uint64_t *out_size) {
    if (base == 0 || out_image == 0 || out_size == 0 ||
        total_size < AGENT_OS_INITRD_HEADER_SIZE) {
        return AGENT_OS_E_INVAL;
    }
    const AgentOsInitrdHeader *header =
        (const AgentOsInitrdHeader *)(const void *)base;
    if (header->magic != AGENT_OS_INITRD_MAGIC ||
        header->version != AGENT_OS_INITRD_VERSION ||
        header->header_size != AGENT_OS_INITRD_HEADER_SIZE ||
        header->total_size != total_size ||
        header->service_count == 0 ||
        header->service_count > AGENT_OS_INITRD_MAX_SERVICES ||
        service_index >= header->service_count ||
        (header->flags & (AGENT_OS_INITRD_FLAG_MANIFEST_JSON |
                          AGENT_OS_INITRD_FLAG_SERVICE_ELF)) !=
            (AGENT_OS_INITRD_FLAG_MANIFEST_JSON |
             AGENT_OS_INITRD_FLAG_SERVICE_ELF) ||
        !agent_os_initrd_range_ok(header->manifest_offset,
                                  header->manifest_size, total_size) ||
        !agent_os_initrd_range_ok(header->service_offset,
                                  header->service_size, total_size) ||
        header->manifest_offset < header->header_size ||
        header->service_offset < header->manifest_offset +
                                  header->manifest_size ||
        header->service_size < header->service_count *
                                   AGENT_OS_INITRD_SERVICE_ENTRY_SIZE) {
        return AGENT_OS_E_INVAL;
    }
    uint32_t table_bytes = header->service_count *
                           AGENT_OS_INITRD_SERVICE_ENTRY_SIZE;
    const AgentOsInitrdServiceEntry *entries =
        (const AgentOsInitrdServiceEntry *)(const void *)
            (base + header->service_offset);
    for (uint32_t index = 0; index < header->service_count; ++index) {
        const AgentOsInitrdServiceEntry *entry = &entries[index];
        if (entry->reserved != 0 || entry->manifest_index >= header->service_count ||
            entry->image_size == 0 ||
            !agent_os_initrd_range_ok(entry->image_offset, entry->image_size,
                                      total_size) ||
            entry->image_offset < header->service_offset + table_bytes ||
            entry->image_offset + entry->image_size >
                header->service_offset + header->service_size) {
            return AGENT_OS_E_INVAL;
        }
    }
    const AgentOsInitrdServiceEntry *selected = &entries[service_index];
    *out_image = base + selected->image_offset;
    *out_size = selected->image_size;
    return AGENT_OS_OK;
}

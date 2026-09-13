#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "initrd.h"

int main(void) {
    uint8_t image[160] = {0};
    AgentOsInitrdHeader *header = (AgentOsInitrdHeader *)image;
    header->magic = AGENT_OS_INITRD_MAGIC;
    header->version = AGENT_OS_INITRD_VERSION;
    header->header_size = AGENT_OS_INITRD_HEADER_SIZE;
    header->total_size = sizeof(image);
    header->manifest_offset = 40;
    header->manifest_size = 1;
    header->service_offset = 48;
    header->service_size = 71;
    header->service_count = 2;
    header->flags = AGENT_OS_INITRD_FLAG_MANIFEST_JSON |
                    AGENT_OS_INITRD_FLAG_SERVICE_ELF;
    AgentOsInitrdServiceEntry *entries =
        (AgentOsInitrdServiceEntry *)(image + header->service_offset);
    entries[0].image_offset = 112;
    entries[0].image_size = 3;
    entries[1].image_offset = 115;
    entries[1].image_size = 4;
    image[112] = 'A'; image[113] = 'A'; image[114] = 'A';
    image[115] = 'B'; image[116] = 'B'; image[117] = 'B'; image[118] = 'B';
    const uint8_t *selected = 0;
    uint64_t selected_size = 0;
    assert(agent_os_initrd_select_service(image, sizeof(image), 1,
                                          &selected, &selected_size) ==
           AGENT_OS_OK);
    assert(selected == image + 115 && selected_size == 4 && selected[0] == 'B');
    assert(agent_os_initrd_select_service(image, sizeof(image), 2,
                                          &selected, &selected_size) !=
           AGENT_OS_OK);
    return 0;
}

#ifndef AGENT_OS_VIRTIO_PCI_H
#define AGENT_OS_VIRTIO_PCI_H

#include <stdint.h>

/* G6 deliberately starts with the transitional legacy virtio transport. */
typedef struct AgentOsVirtioProbe {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t io_base;
    uint16_t queue_size;
    uint16_t tx_queue_size;
    uint8_t queue_ready;
    uint8_t modern_caps;
    uint64_t common_cfg;
    uint64_t notify_cfg;
    uint64_t device_cfg;
} AgentOsVirtioProbe;

typedef struct AgentOsVirtioInputEvent {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} AgentOsVirtioInputEvent;

int agent_os_virtio_probe_block(AgentOsVirtioProbe *out_probe);
int agent_os_virtio_block_read_sector0(const AgentOsVirtioProbe *probe);
int agent_os_virtio_probe_net(AgentOsVirtioProbe *out_probe);
int agent_os_virtio_net_send_test_packet(const AgentOsVirtioProbe *probe);
int agent_os_virtio_net_receive_test_packet(const AgentOsVirtioProbe *probe);
int agent_os_virtio_probe_input(AgentOsVirtioProbe *out_probe);
/* Arms one legacy virtio-input event descriptor.  Returns 1 only after the
 * device advances used.idx and the event buffer is device-visible; returns 0
 * when no host event arrives before the bounded poll expires. */
int agent_os_virtio_input_read_event(const AgentOsVirtioProbe *probe,
                                     AgentOsVirtioInputEvent *out_event);

#endif

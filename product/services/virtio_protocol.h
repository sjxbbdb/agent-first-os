#ifndef AGENT_OS_VIRTIO_PROTOCOL_H
#define AGENT_OS_VIRTIO_PROTOCOL_H

#include <stdint.h>

/* Little-endian, fixed-width service data.  Transport/DMA ownership is not
 * implied by these records; the kernel driver must validate every address. */
enum AgentOsVirtioDeviceType {
    AGENT_OS_VIRTIO_BLOCK = 2,
    AGENT_OS_VIRTIO_NETWORK = 1,
    AGENT_OS_VIRTIO_INPUT = 18,
    AGENT_OS_VIRTIO_SERIAL = 3,
};

enum AgentOsVirtioStatus {
    AGENT_OS_VIRTIO_OK = 0,
    AGENT_OS_VIRTIO_E_BAD_REQUEST = 1,
    AGENT_OS_VIRTIO_E_IO = 2,
    AGENT_OS_VIRTIO_E_NO_DEVICE = 3,
    AGENT_OS_VIRTIO_E_RESET = 4,
};

typedef struct __attribute__((packed)) AgentOsVirtioBlockRequest {
    uint16_t opcode;       /* 0=read, 1=write, 4=flush */
    uint16_t flags;
    uint32_t segment_count;
    uint64_t sector;
    uint64_t buffer_addr;   /* user buffer, validated by service/kernel */
    uint32_t buffer_length;
    uint32_t request_id;
} AgentOsVirtioBlockRequest;

typedef struct __attribute__((packed)) AgentOsVirtioBlockResponse {
    uint32_t request_id;
    uint16_t status;
    uint16_t reserved;
    uint32_t bytes_transferred;
} AgentOsVirtioBlockResponse;

typedef struct __attribute__((packed)) AgentOsVirtioNetFrame {
    uint64_t buffer_addr;
    uint32_t length;
    uint16_t queue;
    uint16_t flags;
    uint64_t timestamp_ns;
} AgentOsVirtioNetFrame;

typedef struct __attribute__((packed)) AgentOsVirtioInputEvent {
    uint16_t event_type;
    uint16_t event_code;
    int32_t value;
    uint64_t timestamp_ns;
} AgentOsVirtioInputEvent;

_Static_assert(sizeof(AgentOsVirtioBlockRequest) == 32,
               "virtio block request ABI changed");
_Static_assert(sizeof(AgentOsVirtioBlockResponse) == 12,
               "virtio block response ABI changed");
_Static_assert(sizeof(AgentOsVirtioNetFrame) == 24,
               "virtio net frame ABI changed");
_Static_assert(sizeof(AgentOsVirtioInputEvent) == 16,
               "virtio input event ABI changed");

#endif

#ifndef AGENT_OS_SERVICE_PROTOCOL_H
#define AGENT_OS_SERVICE_PROTOCOL_H

#include <stdint.h>
#include "abi.h"
#include "capability.h"

enum AgentOsServiceId {
    AGENT_OS_SERVICE_SUPERVISOR = 1,
    AGENT_OS_SERVICE_FILE = 2,
    AGENT_OS_SERVICE_NETWORK = 3,
    AGENT_OS_SERVICE_INPUT = 4,
    AGENT_OS_SERVICE_WINDOW = 5,
    AGENT_OS_SERVICE_POLICY = 6,
    AGENT_OS_SERVICE_REGISTRY = 7,
};

/* Service requests are carried by the normal IPC endpoint.  These opcodes
 * describe the smallest G6 device-facing surface; they do not grant a
 * service access to a device by themselves.  Ring 0 still validates the
 * CapabilityRef and every user buffer before a request reaches a driver. */
enum AgentOsServiceOpcode {
    AGENT_OS_FILE_READ = 0x0201,
    AGENT_OS_FILE_WRITE = 0x0202,
    AGENT_OS_FILE_FLUSH = 0x0203,
    AGENT_OS_NETWORK_SEND = 0x0301,
    AGENT_OS_NETWORK_RECEIVE = 0x0302,
    AGENT_OS_INPUT_POLL = 0x0401,
    AGENT_OS_WINDOW_DELIVER = 0x0501,
};

#define AGENT_OS_SERVICE_MAX_PAYLOAD UINT32_C(4096)

typedef struct AgentOsServiceResponse {
    AbiHeader header;
    uint32_t service_id;
    uint32_t opcode;
    uint64_t request_id;
    uint32_t status;
    uint32_t payload_length;
    uint64_t payload_addr;
    uint32_t bytes_transferred;
    uint32_t flags;
} AgentOsServiceResponse;

/* File operations use a fixed-width range record in the request payload. */
typedef struct AgentOsFileRange {
    uint64_t offset;
    uint32_t length;
    uint32_t reserved;
} AgentOsFileRange;

typedef struct AgentOsServiceRequest {
    AbiHeader header;
    uint32_t service_id;
    uint32_t opcode;
    uint64_t request_id;
    CapabilityRef resource;
    uint64_t payload_addr;
    uint32_t payload_length;
    uint32_t flags;
} AgentOsServiceRequest;

/* Kernel-produced RX completion metadata.  The capability and generation are
 * copied as an opaque pair; Ring 3 must reject stale generations before using
 * the bounded frame buffer. */
typedef struct AgentOsVirtioRxBuffer {
    AbiHeader header;
    uint16_t queue;
    uint16_t descriptor;
    uint32_t reserved;
    CapabilityRef buffer;
    uint64_t address;
    uint32_t length;
    uint32_t flags;
} AgentOsVirtioRxBuffer;

_Static_assert(sizeof(AgentOsServiceRequest) == 56, "service request ABI changed");
_Static_assert(sizeof(AgentOsServiceResponse) == 48, "service response ABI changed");
_Static_assert(sizeof(AgentOsFileRange) == 16, "file range ABI changed");
_Static_assert(sizeof(AgentOsVirtioRxBuffer) == 48, "virtio RX ABI changed");

#endif

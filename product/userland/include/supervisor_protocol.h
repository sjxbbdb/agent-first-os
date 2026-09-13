#ifndef AGENT_OS_SUPERVISOR_PROTOCOL_H
#define AGENT_OS_SUPERVISOR_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define SUPERVISOR_PROTOCOL_VERSION 1u
#define SUPERVISOR_SERVICE_ID_BYTES 64u
#define SUPERVISOR_HEARTBEAT_NONCE_BYTES 32u

typedef enum SupervisorEventKind {
    SUPERVISOR_EVENT_STARTING = 1,
    SUPERVISOR_EVENT_READY = 2,
    SUPERVISOR_EVENT_HEARTBEAT = 3,
    SUPERVISOR_EVENT_FAULT = 4,
    SUPERVISOR_EVENT_EXIT = 5,
    SUPERVISOR_EVENT_FROZEN = 6,
    SUPERVISOR_EVENT_RESTARTING = 7,
    SUPERVISOR_EVENT_STOPPING = 8
} SupervisorEventKind;

/* Serialized field-by-field; this structure is never a remote pointer. */
typedef struct SupervisorEventHeader {
    uint16_t version;
    uint16_t size;
    uint32_t event;
    uint64_t sequence;
    uint64_t monotonic_ns;
    uint64_t generation;
    char service_id[SUPERVISOR_SERVICE_ID_BYTES];
} SupervisorEventHeader;

typedef struct SupervisorHeartbeat {
    SupervisorEventHeader header;
    uint8_t nonce[SUPERVISOR_HEARTBEAT_NONCE_BYTES];
} SupervisorHeartbeat;

_Static_assert(sizeof(SupervisorEventHeader) == 96, "SupervisorEventHeader ABI");
_Static_assert(offsetof(SupervisorEventHeader, service_id) == 32, "service_id offset");
_Static_assert(sizeof(SupervisorHeartbeat) == 128, "SupervisorHeartbeat ABI");

#endif

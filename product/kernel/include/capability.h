#ifndef AGENT_OS_CAPABILITY_H
#define AGENT_OS_CAPABILITY_H

#include <stdint.h>

typedef uint64_t CapabilityHandle;

enum CapabilityRights {
    CAP_RIGHT_READ = UINT64_C(1) << 0,
    CAP_RIGHT_WRITE = UINT64_C(1) << 1,
    CAP_RIGHT_EXEC = UINT64_C(1) << 2,
    CAP_RIGHT_MAP = UINT64_C(1) << 3,
    CAP_RIGHT_SEND = UINT64_C(1) << 4,
    CAP_RIGHT_RECV = UINT64_C(1) << 5,
    CAP_RIGHT_TRANSFER = UINT64_C(1) << 6,
    CAP_RIGHT_REVOKE = UINT64_C(1) << 7,
    CAP_RIGHT_ADMIN = UINT64_C(1) << 8,
};

/* The value is opaque to userland: slot and generation are checked by Ring 0. */
typedef struct CapabilityRef {
    CapabilityHandle handle;
    uint64_t rights;
} CapabilityRef;

_Static_assert(sizeof(CapabilityRef) == 16, "capability reference ABI changed");

#endif

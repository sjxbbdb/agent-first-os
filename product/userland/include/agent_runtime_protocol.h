#ifndef AGENT_OS_AGENT_RUNTIME_PROTOCOL_H
#define AGENT_OS_AGENT_RUNTIME_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "abi.h"
#include "ipc.h"

/* Ring 3 Agent Runtime <-> Supervisor control-plane opcodes.  They are
 * transported by the normal bounded IPC ABI; none of these messages grants
 * a capability or invokes a device. */
enum AgentOsRuntimeOpcode {
    AGENT_OS_RUNTIME_START = 0xD8,
    AGENT_OS_RUNTIME_HEARTBEAT = 0xD9,
    AGENT_OS_RUNTIME_CHECKPOINT = 0xDA,
    AGENT_OS_RUNTIME_ACK = 0xDB,
};

#define AGENT_OS_RUNTIME_TASK_ID_BYTES 32u
#define AGENT_OS_RUNTIME_DIGEST_BYTES 32u

/* Runtime events are exactly the native IPC envelope.  The kernel transports
 * this envelope; semantic fields beyond the bounded inline words must be
 * carried by a separately capability-authorized shared object in a later gate.
 */
typedef IpcMessage AgentOsRuntimeEvent;

/* These aliases make the wire positions explicit instead of presenting a
 * second, incompatible 96-byte struct. */
#define AGENT_OS_RUNTIME_EVENT_SEQUENCE(event) ((event).sequence)
#define AGENT_OS_RUNTIME_EVENT_WORD(event, index) ((event).words[(index)])

_Static_assert(offsetof(AgentOsRuntimeEvent, opcode) == 8,
               "runtime opcode offset changed");
_Static_assert(offsetof(AgentOsRuntimeEvent, sequence) == 24,
               "runtime sequence offset changed");
_Static_assert(offsetof(AgentOsRuntimeEvent, words) == 32,
               "runtime inline word offset changed");
_Static_assert(sizeof(AgentOsRuntimeEvent) == 96,
               "runtime event ABI size changed");
_Static_assert(AGENT_OS_RUNTIME_START == 0xD8 &&
                   AGENT_OS_RUNTIME_HEARTBEAT == 0xD9 &&
                   AGENT_OS_RUNTIME_CHECKPOINT == 0xDA &&
                   AGENT_OS_RUNTIME_ACK == 0xDB,
               "runtime opcode values changed");

#endif

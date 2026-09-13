#ifndef AGENT_OS_PROCESS_H
#define AGENT_OS_PROCESS_H

#include <stdint.h>

#include "interrupts.h"
#include "capability_table.h"
#include "ipc.h"

/* G2 deliberately uses a bounded table.  The kernel can replace this with a
 * slab-backed table later without changing the lifecycle contract. */
#define AGENT_OS_MAX_PROCESSES 32u
#define AGENT_OS_PROCESS_INVALID UINT64_C(0)

typedef uint64_t AgentOsProcessId;

typedef enum AgentOsProcessState {
    AGENT_OS_PROCESS_UNUSED = 0,
    AGENT_OS_PROCESS_READY = 1,
    AGENT_OS_PROCESS_RUNNING = 2,
    AGENT_OS_PROCESS_BLOCKED = 3,
    AGENT_OS_PROCESS_ZOMBIE = 4,
    AGENT_OS_PROCESS_FROZEN = 5,
} AgentOsProcessState;

typedef enum AgentOsProcessResult {
    AGENT_OS_PROCESS_OK = 0,
    AGENT_OS_PROCESS_EINVAL = -1,
    AGENT_OS_PROCESS_ENOSPC = -2,
    AGENT_OS_PROCESS_ENOENT = -3,
    AGENT_OS_PROCESS_EBUSY = -4,
    AGENT_OS_PROCESS_EPERM = -5,
    AGENT_OS_PROCESS_ESTALE = -6,
} AgentOsProcessResult;

typedef struct AgentOsProcessSpec {
    uint64_t entry_rip;
    uint64_t user_rsp;
    uint64_t address_space_root;
    uint64_t kernel_stack_top;
    uint64_t parent;
    uint64_t group_id;
    uint32_t flags;
} AgentOsProcessSpec;

typedef struct AgentOsProcess {
    AgentOsProcessId id;
    AgentOsProcessId parent;
    uint64_t group_id;
    AgentOsProcessState state;
    int64_t exit_code;
    uint64_t address_space_root;
    uint64_t kernel_stack_top;
    uint64_t user_stack_top;
    uint64_t initial_rip;
    uint64_t initial_rsp;
    uint32_t flags;
    uint32_t generation;
    uint8_t has_frame;
    uint8_t reserved[3];
    SyscallFrame frame;
    uint64_t blocked_ipc_buffer;
    uint64_t blocked_ipc_sequence;
    uint64_t blocked_ipc_endpoint;
    CapabilityHandle blocked_ipc_capability;
    uint8_t blocked_ipc_pending;
    uint8_t blocked_ipc_reserved[7];
    IpcMessage blocked_ipc_message;
    /* Each process owns an independent handle namespace.  Kernel object
     * creation and transfer remain explicit operations; zeroed tables are
     * initialized by the process creator when the object is provisioned. */
    AgentOsCapabilityTable capabilities;
} AgentOsProcess;

typedef struct AgentOsProcessTable {
    AgentOsProcess entries[AGENT_OS_MAX_PROCESSES];
    uint32_t current_index;
    uint32_t live_count;
} AgentOsProcessTable;

void agent_os_process_table_init(AgentOsProcessTable *table);
int agent_os_process_create(AgentOsProcessTable *table,
                            const AgentOsProcessSpec *spec,
                            AgentOsProcessId *out_id);
int agent_os_process_lookup(AgentOsProcessTable *table,
                            AgentOsProcessId id,
                            AgentOsProcess **out_process);
int agent_os_process_set_frame(AgentOsProcessTable *table,
                               AgentOsProcessId id,
                               const SyscallFrame *frame);
int agent_os_process_get_frame(const AgentOsProcessTable *table,
                               AgentOsProcessId id,
                               SyscallFrame *out_frame);
int agent_os_process_schedule(AgentOsProcessTable *table,
                              AgentOsProcessId *out_id);
int agent_os_process_yield(AgentOsProcessTable *table);
int agent_os_process_exit(AgentOsProcessTable *table,
                          AgentOsProcessId id,
                          int64_t exit_code);
int agent_os_process_kill(AgentOsProcessTable *table,
                          AgentOsProcessId id,
                          int64_t exit_code);
/* Terminate direct children in one kernel-owned group.  The caller must be
 * the live parent; descendants and unrelated groups are never touched. */
int agent_os_process_group_terminate(AgentOsProcessTable *table,
                                     AgentOsProcessId parent,
                                     uint64_t group_id,
                                     int64_t exit_code);
int agent_os_process_group_terminate_tree(AgentOsProcessTable *table,
                                          AgentOsProcessId parent,
                                          uint64_t group_id,
                                          int64_t exit_code);
int agent_os_process_group_freeze(AgentOsProcessTable *table,
                                  AgentOsProcessId parent, uint64_t group_id);
int agent_os_process_group_resume(AgentOsProcessTable *table,
                                  AgentOsProcessId parent, uint64_t group_id);
int agent_os_process_cancel_ipc(AgentOsProcessTable *table,
                                AgentOsProcessId id);
int agent_os_process_restart(AgentOsProcessTable *table,
                             AgentOsProcessId id);
int agent_os_process_wait(AgentOsProcessTable *table,
                          AgentOsProcessId parent,
                          AgentOsProcessId child,
                          int64_t *out_exit_code,
                          AgentOsProcessId *out_reaped);
AgentOsProcessId agent_os_process_current(const AgentOsProcessTable *table);

#endif

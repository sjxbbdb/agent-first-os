#include "process.h"

static AgentOsProcessId make_id(uint32_t index, uint32_t generation) {
    return ((uint64_t)generation << 32) | ((uint64_t)index + 1u);
}

static uint32_t id_index(AgentOsProcessId id) {
    uint32_t raw = (uint32_t)id;
    return raw == 0 ? UINT32_MAX : raw - 1u;
}

static uint32_t id_generation(AgentOsProcessId id) {
    return (uint32_t)(id >> 32);
}

static AgentOsProcess *find_process(AgentOsProcessTable *table,
                                    AgentOsProcessId id) {
    uint32_t index = id_index(id);
    if (table == 0 || index >= AGENT_OS_MAX_PROCESSES || id_generation(id) == 0) {
        return 0;
    }
    AgentOsProcess *process = &table->entries[index];
    if (process->state == AGENT_OS_PROCESS_UNUSED ||
        process->generation != id_generation(id) || process->id != id) {
        return 0;
    }
    return process;
}

static void clear_process(AgentOsProcess *process) {
    uint32_t next_generation = process->generation + 1u;
    if (next_generation == 0) {
        next_generation = 1;
    }
    for (uint32_t offset = 0; offset < sizeof(*process); ++offset) {
        ((uint8_t *)process)[offset] = 0;
    }
    process->generation = next_generation;
    process->state = AGENT_OS_PROCESS_UNUSED;
}

static void revoke_process_capabilities(AgentOsProcess *process) {
    if (process == 0) return;
    for (uint32_t i = 0; i < AGENT_OS_CAPABILITY_SLOTS; ++i) {
        AgentOsCapabilityEntry *entry = &process->capabilities.entries[i];
        if (!entry->active) continue;
        entry->active = 0;
        entry->object = 0;
        entry->rights = 0;
        entry->parent_table = 0;
        entry->parent_incarnation = 0;
        entry->parent_handle = 0;
        entry->depth = 0;
        entry->generation = entry->generation == UINT32_MAX ? 0 : entry->generation + 1u;
    }
}

void agent_os_process_table_init(AgentOsProcessTable *table) {
    if (table == 0) {
        return;
    }
    for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
        for (uint32_t offset = 0; offset < sizeof(table->entries[index]); ++offset) {
            ((uint8_t *)&table->entries[index])[offset] = 0;
        }
        table->entries[index].generation = 1;
        table->entries[index].state = AGENT_OS_PROCESS_UNUSED;
    }
    table->current_index = UINT32_MAX;
    table->live_count = 0;
}

int agent_os_process_create(AgentOsProcessTable *table,
                            const AgentOsProcessSpec *spec,
                            AgentOsProcessId *out_id) {
    if (table == 0 || spec == 0 || out_id == 0 ||
        spec->entry_rip == 0 || spec->user_rsp == 0) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    if (spec->parent != AGENT_OS_PROCESS_INVALID) {
        AgentOsProcess *parent = find_process(table, spec->parent);
        if (parent == 0) {
            return AGENT_OS_PROCESS_ENOENT;
        }
        if (parent->state == AGENT_OS_PROCESS_ZOMBIE) {
            return AGENT_OS_PROCESS_EPERM;
        }
    }
    for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
        AgentOsProcess *process = &table->entries[index];
        if (process->state != AGENT_OS_PROCESS_UNUSED) {
            continue;
        }
        uint32_t generation = process->generation == 0 ? 1 : process->generation;
        process->generation = generation;
        process->id = make_id(index, generation);
        process->parent = spec->parent;
        process->group_id = spec->group_id;
        process->state = AGENT_OS_PROCESS_READY;
        process->address_space_root = spec->address_space_root;
        process->kernel_stack_top = spec->kernel_stack_top;
        process->user_stack_top = spec->user_rsp;
        process->initial_rip = spec->entry_rip;
        process->initial_rsp = spec->user_rsp;
        process->flags = spec->flags;
        process->exit_code = 0;
        process->frame = (SyscallFrame){0};
        process->frame.rip = spec->entry_rip;
        process->frame.cs = 0x2B;     /* ring-3 64-bit code selector */
        process->frame.rflags = 0x202; /* IF=1, reserved bit set */
        process->frame.rsp = spec->user_rsp;
        process->frame.ss = 0x23;     /* ring-3 data selector */
        process->has_frame = 1;
        table->live_count += 1;
        *out_id = process->id;
        return AGENT_OS_PROCESS_OK;
    }
    return AGENT_OS_PROCESS_ENOSPC;
}

int agent_os_process_lookup(AgentOsProcessTable *table,
                            AgentOsProcessId id,
                            AgentOsProcess **out_process) {
    if (table == 0 || out_process == 0 || id == AGENT_OS_PROCESS_INVALID) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    AgentOsProcess *process = find_process(table, id);
    if (process == 0) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    *out_process = process;
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_set_frame(AgentOsProcessTable *table,
                               AgentOsProcessId id,
                               const SyscallFrame *frame) {
    AgentOsProcess *process;
    if (frame == 0) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    int lookup = agent_os_process_lookup(table, id, &process);
    if (lookup != AGENT_OS_PROCESS_OK) {
        return lookup;
    }
    process->frame = *frame;
    process->has_frame = 1;
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_get_frame(const AgentOsProcessTable *table,
                               AgentOsProcessId id,
                               SyscallFrame *out_frame) {
    uint32_t index = id_index(id);
    if (table == 0 || out_frame == 0 || index >= AGENT_OS_MAX_PROCESSES ||
        id_generation(id) == 0) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    const AgentOsProcess *process = &table->entries[index];
    if (process->state == AGENT_OS_PROCESS_UNUSED ||
        process->generation != id_generation(id) || process->id != id) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    if (!process->has_frame) {
        return AGENT_OS_PROCESS_ENOENT;
    }
    *out_frame = process->frame;
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_schedule(AgentOsProcessTable *table,
                              AgentOsProcessId *out_id) {
    if (table == 0 || out_id == 0) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    uint32_t start = table->current_index == UINT32_MAX ? 0 :
                     (table->current_index + 1u) % AGENT_OS_MAX_PROCESSES;
    for (uint32_t offset = 0; offset < AGENT_OS_MAX_PROCESSES; ++offset) {
        uint32_t index = (start + offset) % AGENT_OS_MAX_PROCESSES;
        AgentOsProcess *candidate = &table->entries[index];
        if (candidate->state != AGENT_OS_PROCESS_READY) {
            continue;
        }
        if (table->current_index < AGENT_OS_MAX_PROCESSES &&
            table->entries[table->current_index].state == AGENT_OS_PROCESS_RUNNING) {
            table->entries[table->current_index].state = AGENT_OS_PROCESS_READY;
        }
        candidate->state = AGENT_OS_PROCESS_RUNNING;
        table->current_index = index;
        *out_id = candidate->id;
        return AGENT_OS_PROCESS_OK;
    }
    return AGENT_OS_PROCESS_EBUSY;
}

int agent_os_process_yield(AgentOsProcessTable *table) {
    if (table == 0 || table->current_index >= AGENT_OS_MAX_PROCESSES) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    AgentOsProcess *current = &table->entries[table->current_index];
    if (current->state != AGENT_OS_PROCESS_RUNNING) {
        return AGENT_OS_PROCESS_EBUSY;
    }
    current->state = AGENT_OS_PROCESS_READY;
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_exit(AgentOsProcessTable *table,
                          AgentOsProcessId id,
                          int64_t exit_code) {
    AgentOsProcess *process;
    if (agent_os_process_lookup(table, id, &process) != 0) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    if (process->state == AGENT_OS_PROCESS_ZOMBIE) {
        return AGENT_OS_PROCESS_EBUSY;
    }
    process->exit_code = exit_code;
    process->state = AGENT_OS_PROCESS_ZOMBIE;
    if (table->current_index < AGENT_OS_MAX_PROCESSES &&
        &table->entries[table->current_index] == process) {
        table->current_index = UINT32_MAX;
    }
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_kill(AgentOsProcessTable *table,
                          AgentOsProcessId id,
                          int64_t exit_code) {
    AgentOsProcess *process;
    if (agent_os_process_lookup(table, id, &process) != AGENT_OS_PROCESS_OK) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    revoke_process_capabilities(process);
    return agent_os_process_exit(table, id, exit_code);
}

int agent_os_process_group_terminate(AgentOsProcessTable *table,
                                     AgentOsProcessId parent,
                                     uint64_t group_id,
                                     int64_t exit_code) {
    if (table == 0 || parent == AGENT_OS_PROCESS_INVALID ||
        find_process(table, parent) == 0) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    int count = 0;
    for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
        AgentOsProcess *process = &table->entries[index];
        if (process->state == AGENT_OS_PROCESS_UNUSED ||
            process->state == AGENT_OS_PROCESS_ZOMBIE ||
            process->parent != parent || process->group_id != group_id) {
            continue;
        }
        if (agent_os_process_exit(table, process->id, exit_code) ==
            AGENT_OS_PROCESS_OK) {
            ++count;
        }
    }
    return count;
}

static int descendant_of(AgentOsProcessTable *table, AgentOsProcessId root,
                         AgentOsProcessId candidate) {
    AgentOsProcess *node = find_process(table, candidate);
    for (uint32_t depth = 0; node != 0 && depth < AGENT_OS_MAX_PROCESSES; ++depth) {
        if (node->parent == root) return 1;
        if (node->parent == AGENT_OS_PROCESS_INVALID) return 0;
        node = find_process(table, node->parent);
    }
    return 0;
}

int agent_os_process_group_terminate_tree(AgentOsProcessTable *table,
                                          AgentOsProcessId parent,
                                          uint64_t group_id,
                                          int64_t exit_code) {
    if (table == 0 || parent == AGENT_OS_PROCESS_INVALID ||
        find_process(table, parent) == 0) return AGENT_OS_PROCESS_ESTALE;
    int count = 0;
    for (uint32_t i = 0; i < AGENT_OS_MAX_PROCESSES; ++i) {
        AgentOsProcess *p = &table->entries[i];
        if (p->state == AGENT_OS_PROCESS_UNUSED || p->state == AGENT_OS_PROCESS_ZOMBIE ||
            p->group_id != group_id || !descendant_of(table, parent, p->id)) continue;
        if (agent_os_process_exit(table, p->id, exit_code) == AGENT_OS_PROCESS_OK) ++count;
    }
    return count;
}

static int group_transition(AgentOsProcessTable *table,
                            AgentOsProcessId parent, uint64_t group_id,
                            AgentOsProcessState from,
                            AgentOsProcessState to) {
    if (table == 0 || parent == AGENT_OS_PROCESS_INVALID ||
        find_process(table, parent) == 0) return AGENT_OS_PROCESS_ESTALE;
    int count = 0;
    for (uint32_t i = 0; i < AGENT_OS_MAX_PROCESSES; ++i) {
        AgentOsProcess *p = &table->entries[i];
        if (p->state == from && p->parent == parent && p->group_id == group_id) {
            p->state = to; ++count;
        }
    }
    return count;
}

int agent_os_process_group_freeze(AgentOsProcessTable *table,
                                  AgentOsProcessId parent, uint64_t group_id) {
    return group_transition(table, parent, group_id, AGENT_OS_PROCESS_READY,
                            AGENT_OS_PROCESS_FROZEN);
}

int agent_os_process_group_resume(AgentOsProcessTable *table,
                                  AgentOsProcessId parent, uint64_t group_id) {
    return group_transition(table, parent, group_id, AGENT_OS_PROCESS_FROZEN,
                            AGENT_OS_PROCESS_READY);
}

int agent_os_process_cancel_ipc(AgentOsProcessTable *table,
                                AgentOsProcessId id) {
    AgentOsProcess *process;
    if (agent_os_process_lookup(table, id, &process) != AGENT_OS_PROCESS_OK) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    if (process->state != AGENT_OS_PROCESS_BLOCKED ||
        process->blocked_ipc_buffer == 0) {
        return AGENT_OS_PROCESS_EBUSY;
    }
    process->frame.rax = (uint64_t)-125; /* ECANCELED */
    process->blocked_ipc_buffer = 0;
    process->blocked_ipc_sequence = 0;
    process->blocked_ipc_endpoint = 0;
    process->blocked_ipc_capability = 0;
    process->blocked_ipc_pending = 0;
    process->state = AGENT_OS_PROCESS_READY;
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_restart(AgentOsProcessTable *table,
                             AgentOsProcessId id) {
    AgentOsProcess *process;
    if (table == 0 || agent_os_process_lookup(table, id, &process) !=
            AGENT_OS_PROCESS_OK) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    if (process->state != AGENT_OS_PROCESS_ZOMBIE ||
        process->initial_rip == 0 || process->initial_rsp == 0) {
        return AGENT_OS_PROCESS_EBUSY;
    }
    process->exit_code = 0;
    process->frame = (SyscallFrame){0};
    process->blocked_ipc_buffer = 0;
    process->blocked_ipc_sequence = 0;
    process->blocked_ipc_endpoint = 0;
    process->blocked_ipc_capability = 0;
    process->blocked_ipc_pending = 0;
    process->blocked_ipc_message = (IpcMessage){0};
    process->frame.rip = process->initial_rip;
    process->frame.cs = 0x2B;
    process->frame.rflags = 0x202;
    process->frame.rsp = process->initial_rsp;
    process->frame.ss = 0x23;
    process->has_frame = 1;
    process->state = AGENT_OS_PROCESS_READY;
    if (table->live_count != 0) {
        /* The zombie remained accounted for until a wait/reap. */
    } else {
        table->live_count = 1;
    }
    return AGENT_OS_PROCESS_OK;
}

int agent_os_process_wait(AgentOsProcessTable *table,
                          AgentOsProcessId parent,
                          AgentOsProcessId child,
                          int64_t *out_exit_code,
                          AgentOsProcessId *out_reaped) {
    if (table == 0 || out_exit_code == 0 || out_reaped == 0) {
        return AGENT_OS_PROCESS_EINVAL;
    }
    if (parent != AGENT_OS_PROCESS_INVALID && find_process(table, parent) == 0) {
        return AGENT_OS_PROCESS_ESTALE;
    }
    for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
        AgentOsProcess *process = &table->entries[index];
        if (process->state != AGENT_OS_PROCESS_ZOMBIE || process->parent != parent ||
            (child != AGENT_OS_PROCESS_INVALID && process->id != child)) {
            continue;
        }
        AgentOsProcessId reaped = process->id;
        *out_exit_code = process->exit_code;
        *out_reaped = reaped;
        clear_process(process);
        if (table->live_count != 0) {
            table->live_count -= 1;
        }
        return AGENT_OS_PROCESS_OK;
    }
    return AGENT_OS_PROCESS_EBUSY;
}

AgentOsProcessId agent_os_process_current(const AgentOsProcessTable *table) {
    if (table == 0 || table->current_index >= AGENT_OS_MAX_PROCESSES) {
        return AGENT_OS_PROCESS_INVALID;
    }
    const AgentOsProcess *process = &table->entries[table->current_index];
    return process->state == AGENT_OS_PROCESS_RUNNING ? process->id :
           AGENT_OS_PROCESS_INVALID;
}

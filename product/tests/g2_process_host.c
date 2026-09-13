#include <assert.h>
#include <stdio.h>

#include "process.h"

static AgentOsProcessSpec spec(AgentOsProcessId parent, uint64_t entry) {
    AgentOsProcessSpec value = {0};
    value.parent = parent;
    value.entry_rip = entry;
    value.user_rsp = 0x700000;
    value.address_space_root = 0x1000;
    value.kernel_stack_top = 0x900000;
    return value;
}

int main(void) {
    AgentOsProcessTable table;
    AgentOsProcessId parent, child, second, selected;
    AgentOsProcessSpec parent_spec = spec(0, 0x1000);
    AgentOsProcessSpec child_spec;
    AgentOsProcessSpec second_spec;
    AgentOsProcess *stale;
    int64_t code;
    AgentOsProcessId reaped;
    SyscallFrame frame = {0};
    SyscallFrame copied;

    agent_os_process_table_init(&table);
    assert(agent_os_process_create(&table, &parent_spec, &parent) == 0);
    child_spec = spec(parent, 0x2000);
    second_spec = spec(parent, 0x3000);
    assert(agent_os_process_create(&table, &child_spec, &child) == 0);
    assert(agent_os_process_create(&table, &second_spec, &second) == 0);
    assert(parent != child && child != second);

    assert(agent_os_process_get_frame(&table, parent, &copied) == 0);
    assert(copied.rip == 0x1000 && copied.rsp == 0x700000);

    frame.rip = 0x2000;
    frame.rsp = 0x700000;
    assert(agent_os_process_set_frame(&table, child, &frame) == 0);
    assert(agent_os_process_get_frame(&table, child, &copied) == 0);
    assert(copied.rip == frame.rip && copied.rsp == frame.rsp);

    assert(agent_os_process_schedule(&table, &selected) == 0);
    assert(selected == parent);
    assert(agent_os_process_yield(&table) == 0);
    assert(agent_os_process_schedule(&table, &selected) == 0);
    assert(selected == child);
    assert(agent_os_process_exit(&table, child, 42) == 0);
    assert(agent_os_process_wait(&table, parent, child, &code, &reaped) == 0);
    assert(code == 42 && reaped == child);
    assert(agent_os_process_lookup(&table, child, &stale) != 0);

    AgentOsProcessId replacement;
    child_spec = spec(parent, 0x4000);
    assert(agent_os_process_create(&table, &child_spec, &replacement) == 0);
    assert(replacement != child);
    assert(agent_os_process_lookup(&table, child, &stale) != 0);

    assert(agent_os_process_kill(&table, second, -9) == 0);
    assert(agent_os_process_wait(&table, parent, 0, &code, &reaped) == 0);
    assert(code == -9 && reaped == second);
    assert(agent_os_process_wait(&table, parent, 0, &code, &reaped) != 0);
    puts("G2 process lifecycle OK");
    return 0;
}

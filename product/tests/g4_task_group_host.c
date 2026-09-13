#include <assert.h>
#include <stdio.h>

#include "process.h"

static AgentOsProcessSpec spec(AgentOsProcessId parent, uint64_t group) {
    AgentOsProcessSpec value = {0};
    value.parent = parent;
    value.group_id = group;
    value.entry_rip = 0x1000 + group;
    value.user_rsp = 0x700000;
    return value;
}

int main(void) {
    AgentOsProcessTable table;
    AgentOsProcessId parent, child, peer;
    AgentOsProcessId reaped;
    int64_t code;
    AgentOsProcessSpec value;
    agent_os_process_table_init(&table);
    value = spec(0, 1);
    assert(agent_os_process_create(&table, &value, &parent) == 0);
    value = spec(parent, 7);
    assert(agent_os_process_create(&table, &value, &child) == 0);
    value = spec(parent, 8);
    assert(agent_os_process_create(&table, &value, &peer) == 0);
    assert(agent_os_process_group_terminate(&table, parent, 7, -15) == 1);
    assert(agent_os_process_wait(&table, parent, child, &code, &reaped) == 0);
    assert(reaped == child && code == -15);
    assert(agent_os_process_lookup(&table, peer, &(AgentOsProcess *){0}) == 0);
    assert(agent_os_process_group_terminate(&table, peer, 7, -15) == 0);
    puts("G4 native task-group lifecycle OK");
    return 0;
}

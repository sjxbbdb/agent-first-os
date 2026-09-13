#include <assert.h>
#include <stdio.h>
#include "process.h"

int main(void) {
    AgentOsProcessTable table;
    AgentOsProcessId parent, child;
    AgentOsProcess *parent_record, *child_record;
    CapabilityHandle source, old_handle, fresh_handle;
    uint64_t object;
    AgentOsProcessSpec spec = { .entry_rip = 0x1000, .user_rsp = 0x700000 };
    agent_os_process_table_init(&table);
    assert(agent_os_process_create(&table, &spec, &parent) == 0);
    spec.parent = parent;
    assert(agent_os_process_create(&table, &spec, &child) == 0);
    assert(agent_os_process_lookup(&table, parent, &parent_record) == 0);
    assert(agent_os_process_lookup(&table, child, &child_record) == 0);
    agent_os_capability_table_init(&parent_record->capabilities);
    agent_os_capability_table_init(&child_record->capabilities);
    assert(agent_os_capability_mint(&parent_record->capabilities, 0xCAFE,
                                    CAP_RIGHT_SEND | CAP_RIGHT_TRANSFER,
                                    &source) == 0);
    assert(agent_os_capability_mint(&child_record->capabilities, 0xCAFE,
                                    CAP_RIGHT_SEND, &old_handle) == 0);
    assert(agent_os_process_clean_exit(&table, child, 0) == 0);
    assert(agent_os_capability_lookup(&child_record->capabilities, old_handle,
                                      CAP_RIGHT_SEND, &object, 0) != 0);
    assert(agent_os_process_restart(&table, child) == 0);
    assert(agent_os_capability_lookup(&child_record->capabilities, old_handle,
                                      CAP_RIGHT_SEND, &object, 0) != 0);
    assert(agent_os_capability_transfer(&parent_record->capabilities, source,
                                        &child_record->capabilities,
                                        CAP_RIGHT_SEND, &fresh_handle) == 0);
    assert(fresh_handle != old_handle);
    assert(agent_os_capability_lookup(&child_record->capabilities, fresh_handle,
                                      CAP_RIGHT_SEND, &object, 0) == 0);
    puts("G4 clean-exit restart capability re-mint OK");
    return 0;
}

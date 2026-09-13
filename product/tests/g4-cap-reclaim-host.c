#include <assert.h>
#include <stdio.h>
#include "process.h"
int main(void) {
    AgentOsProcessTable t; AgentOsProcessId p; AgentOsProcess *proc; CapabilityHandle h;
    agent_os_process_table_init(&t);
    AgentOsProcessSpec s = { .entry_rip=0x1000, .user_rsp=0x700000 };
    assert(agent_os_process_create(&t,&s,&p)==0);
    assert(agent_os_process_lookup(&t,p,&proc)==0);
    agent_os_capability_table_init(&proc->capabilities);
    assert(agent_os_capability_mint(&proc->capabilities,0x1234,CAP_RIGHT_SEND,&h)==0);
    assert(agent_os_capability_lookup(&proc->capabilities,h,CAP_RIGHT_SEND,&(uint64_t){0},0)==0);
    assert(agent_os_process_kill(&t,p,0)==0);
    assert(agent_os_capability_lookup(&proc->capabilities,h,CAP_RIGHT_SEND,&(uint64_t){0},0)!=0);
    puts("G4 process capability reclaim OK"); return 0;
}

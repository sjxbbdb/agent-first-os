#include <assert.h>
#include <stdio.h>
#include "process.h"
static AgentOsProcessSpec s(AgentOsProcessId p, uint64_t g, uint64_t rip) {
    AgentOsProcessSpec x = {0}; x.parent=p; x.group_id=g; x.entry_rip=rip; x.user_rsp=0x700000; return x;
}
int main(void) {
    AgentOsProcessTable t; AgentOsProcessId p,c; AgentOsProcess *q;
    AgentOsProcessSpec x; agent_os_process_table_init(&t);
    x=s(0,1,0x1000); assert(agent_os_process_create(&t,&x,&p)==0);
    x=s(p,9,0x2000); assert(agent_os_process_create(&t,&x,&c)==0);
    assert(agent_os_process_group_freeze(&t,p,9)==1);
    assert(agent_os_process_lookup(&t,c,&q)==0 && q->state==AGENT_OS_PROCESS_FROZEN);
    assert(agent_os_process_schedule(&t,&c)==0 && c==p);
    assert(agent_os_process_yield(&t)==0);
    assert(agent_os_process_schedule(&t,&c)==0 && c==p);
    assert(agent_os_process_yield(&t)==0);
    assert(agent_os_process_group_resume(&t,p,9)==1);
    assert(agent_os_process_schedule(&t,&c)==0);
    assert(agent_os_process_yield(&t)==0);
    assert(agent_os_process_schedule(&t,&c)==0);
    puts("G4 task-group freeze/resume OK"); return 0;
}

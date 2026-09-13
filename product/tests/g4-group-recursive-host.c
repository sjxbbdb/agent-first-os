#include <assert.h>
#include <stdio.h>
#include "process.h"
static AgentOsProcessSpec s(AgentOsProcessId p,uint64_t g,uint64_t r){AgentOsProcessSpec x={0};x.parent=p;x.group_id=g;x.entry_rip=r;x.user_rsp=0x700000;return x;}
int main(void){AgentOsProcessTable t;AgentOsProcessId root,child,grand,other;AgentOsProcessSpec x;AgentOsProcess *p;agent_os_process_table_init(&t);
 x=s(0,1,0x1000);assert(agent_os_process_create(&t,&x,&root)==0);x=s(root,9,0x2000);assert(agent_os_process_create(&t,&x,&child)==0);x=s(child,9,0x3000);assert(agent_os_process_create(&t,&x,&grand)==0);x=s(root,8,0x4000);assert(agent_os_process_create(&t,&x,&other)==0);
 assert(agent_os_process_group_terminate_tree(&t,root,9,-9)==2);assert(agent_os_process_lookup(&t,child,&p)==0&&p->state==AGENT_OS_PROCESS_ZOMBIE);assert(agent_os_process_lookup(&t,grand,&p)==0&&p->state==AGENT_OS_PROCESS_ZOMBIE);assert(agent_os_process_lookup(&t,other,&p)==0&&p->state!=AGENT_OS_PROCESS_ZOMBIE);puts("G4 recursive task-group terminate OK");}

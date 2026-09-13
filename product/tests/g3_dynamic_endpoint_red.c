#include "ipc_endpoint.h"
int main(void) { AgentOsIpcEndpoint e; return agent_os_ipc_endpoint_create(&e) != 0; }

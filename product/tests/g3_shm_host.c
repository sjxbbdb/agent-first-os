#include <assert.h>
#include <stdio.h>

#include "shared_memory.h"

int main(void) {
    AgentOsSharedMemoryTable table;
    AgentOsSharedMemoryHandle source;
    AgentOsSharedMemoryHandle child;
    uint64_t base;
    uint64_t length;
    uint64_t rights;

    assert(agent_os_shm_create(0, 0x1000, 0x1000, CAP_RIGHT_READ, &source) ==
           AGENT_OS_E_INVAL);
    agent_os_shm_table_init(&table);
    assert(agent_os_shm_create(&table, 0x1000, 0x4000,
                               CAP_RIGHT_READ | CAP_RIGHT_WRITE |
                                   CAP_RIGHT_MAP,
                               &source) == AGENT_OS_OK);
    assert(agent_os_shm_lookup(&table, source, CAP_RIGHT_READ, &base, &length,
                               &rights) == AGENT_OS_OK);
    assert(base == 0x1000 && length == 0x4000 &&
           rights == (CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_MAP));

    assert(agent_os_shm_create(&table, 0x1800, 0x1000, CAP_RIGHT_READ,
                               &child) == AGENT_OS_E_INVAL);
    assert(agent_os_shm_create(&table, 0x1000, 0, CAP_RIGHT_READ, &child) ==
           AGENT_OS_E_INVAL);
    assert(agent_os_shm_create(&table, UINT64_MAX - 0xFFF, 0x2000,
                               CAP_RIGHT_READ, &child) == AGENT_OS_E_INVAL);
    assert(agent_os_shm_restrict(&table, source, 0x1000, 0x1000,
                                 CAP_RIGHT_READ, &child) == AGENT_OS_OK);
    assert(agent_os_shm_lookup(&table, child, CAP_RIGHT_READ, &base, &length,
                               &rights) == AGENT_OS_OK);
    assert(base == 0x2000 && length == 0x1000 && rights == CAP_RIGHT_READ);
    assert(agent_os_shm_restrict(&table, source, 0, 0x1000, CAP_RIGHT_EXEC,
                                 &child) == AGENT_OS_E_DENIED);
    assert(agent_os_shm_restrict(&table, source, 0x3000, 0x2000,
                                 CAP_RIGHT_READ, &child) == AGENT_OS_E_DENIED);
    assert(agent_os_shm_revoke(&table, source) == AGENT_OS_OK);
    assert(agent_os_shm_lookup(&table, source, 0, &base, &length, 0) ==
           AGENT_OS_E_REVOKED);
    puts("G3 shared-memory metadata OK");
    return 0;
}

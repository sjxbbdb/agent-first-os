#include <assert.h>
#include <stdio.h>
#include "service_protocol.h"
int main(void) {
    AgentOsVirtioRxBuffer x = {0};
    x.header.version = AGENT_OS_ABI_VERSION; x.header.size = sizeof(x);
    x.queue = 0; x.descriptor = 3; x.buffer.handle = 0x100000003; x.buffer.rights = CAP_RIGHT_WRITE;
    x.address = 0x400000; x.length = 1514;
    assert(x.header.size == 48 && x.queue == 0 && x.length <= 1514);
    puts("G6 native RX handoff ABI layout OK"); return 0;
}

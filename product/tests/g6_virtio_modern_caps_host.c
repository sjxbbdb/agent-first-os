#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct Caps {
    uint8_t common;
    uint8_t notify;
    uint8_t device;
} Caps;

static int parse_caps(const uint8_t *cfg, size_t size, Caps *out) {
    if (!cfg || !out || size < 0x40) return 0;
    memset(out, 0, sizeof(*out));
    uint8_t at = cfg[0x34];
    for (unsigned count = 0; at >= 0x40 && (size_t)at + 16u <= size && count < 32; ++count) {
        uint8_t next = cfg[at + 1];
        if (cfg[at] == 0x09) {
            uint8_t type = cfg[at + 3];
            uint8_t bar = cfg[at + 4];
            uint32_t offset = (uint32_t)cfg[at + 8] |
                              ((uint32_t)cfg[at + 9] << 8) |
                              ((uint32_t)cfg[at + 10] << 16) |
                              ((uint32_t)cfg[at + 11] << 24);
            uint32_t length = (uint32_t)cfg[at + 12] |
                              ((uint32_t)cfg[at + 13] << 8) |
                              ((uint32_t)cfg[at + 14] << 16) |
                              ((uint32_t)cfg[at + 15] << 24);
            if (bar >= 6 || length == 0 || offset > UINT32_MAX - length) return 0;
            if (type == 1) out->common = 1;
            else if (type == 2) out->notify = 1;
            else if (type == 4) out->device = 1;
        }
        if (next == 0) break;
        if (next <= at || (size_t)next + 16u > size) return 0;
        at = next;
    }
    return out->common && out->notify;
}

int main(void) {
    uint8_t cfg[256] = {0};
    Caps caps;
    cfg[0x34] = 0x40;
    cfg[0x40] = 0x09; cfg[0x41] = 0x50; cfg[0x43] = 1; cfg[0x44] = 2; cfg[0x4c] = 0x20;
    cfg[0x50] = 0x09; cfg[0x51] = 0x60; cfg[0x53] = 2; cfg[0x54] = 4; cfg[0x5c] = 0x10;
    assert(parse_caps(cfg, sizeof(cfg), &caps) && caps.common && caps.notify);
    cfg[0x54] = 6; assert(!parse_caps(cfg, sizeof(cfg), &caps));
    cfg[0x54] = 4; cfg[0x51] = 0x40; assert(!parse_caps(cfg, sizeof(cfg), &caps));
    cfg[0x51] = 0; cfg[0x48] = 0xff; cfg[0x4c] = 0xff; cfg[0x4d] = 0xff; cfg[0x4e] = 0xff; cfg[0x4f] = 0xff;
    assert(!parse_caps(cfg, sizeof(cfg), &caps));
    return 0;
}

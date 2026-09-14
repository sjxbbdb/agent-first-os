#include "virtio_pci.h"
#include "vm.h"

static uint8_t queue_page[16384] __attribute__((aligned(4096)));
static uint8_t net_tx_queue_page[16384] __attribute__((aligned(4096)));
static uint8_t input_queue_page[16384] __attribute__((aligned(4096)));
static uint8_t request_page[4096] __attribute__((aligned(4096)));
static uint8_t net_tx_page[4096] __attribute__((aligned(4096)));
static uint8_t input_event_page[4096] __attribute__((aligned(4096)));

typedef struct __attribute__((packed)) VirtioDescriptor {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VirtioDescriptor;

typedef struct __attribute__((packed)) VirtioBlkRequest {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} VirtioBlkRequest;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}


static uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                                  uint8_t function, uint8_t offset) {
    uint32_t address = UINT32_C(0x80000000) |
        ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
        ((uint32_t)function << 8) | (offset & 0xfcu);
    outl(0xcf8, address);
    return inl(0xcfc);
}

static uint8_t pci_config_read8(uint8_t bus, uint8_t device,
                                uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, device, function, offset);
    return (uint8_t)(value >> ((offset & 3u) * 8u));
}

static uint64_t pci_bar_address(uint8_t bus, uint8_t device,
                                uint8_t function, uint8_t bar) {
    uint8_t offset = (uint8_t)(0x10u + bar * 4u);
    uint32_t low = pci_config_read32(bus, device, function, offset);
    if ((low & 1u) != 0) return 0;
    if ((low & 6u) == 0x4u) {
        uint32_t high = pci_config_read32(bus, device, function,
                                          (uint8_t)(offset + 4u));
        return ((uint64_t)high << 32) | (low & ~UINT32_C(0xf));
    }
    return low & ~UINT32_C(0xf);
}

static int discover_input_modern(AgentOsVirtioProbe *out_probe) {
    for (uint16_t bus = 0; bus < 32; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            uint32_t id = pci_config_read32((uint8_t)bus, device, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xffffu);
            uint16_t product = (uint16_t)(id >> 16);
            if (vendor != 0x1af4 ||
                (product != 0x1052 && product != 0x1053 && product != 0x1054)) continue;
            uint8_t capability = pci_config_read8((uint8_t)bus, device, 0, 0x34);
            while (capability >= 0x40 && capability < 0xfc) {
                uint8_t id_byte = pci_config_read8((uint8_t)bus, device, 0, capability);
                uint8_t next = pci_config_read8((uint8_t)bus, device, 0,
                                                 (uint8_t)(capability + 1));
                if (id_byte == 0x09) {
                    uint8_t cfg_type = pci_config_read8((uint8_t)bus, device, 0,
                                                         (uint8_t)(capability + 3));
                    uint8_t bar = pci_config_read8((uint8_t)bus, device, 0,
                                                   (uint8_t)(capability + 4));
                    uint32_t offset = pci_config_read32((uint8_t)bus, device, 0,
                                                        (uint8_t)(capability + 8));
                    uint32_t multiplier = pci_config_read32((uint8_t)bus, device, 0,
                                                             (uint8_t)(capability + 16));
                    uint64_t address = cfg_type == 5
                        ? 0 : pci_bar_address((uint8_t)bus, device, 0, bar);
                    if (cfg_type != 5 && address == 0) return 0;
                    address += offset;
                    if (cfg_type == 1) out_probe->common_cfg = address;
                    if (cfg_type == 2) out_probe->notify_cfg = address;
                    if (cfg_type == 4) out_probe->device_cfg = address;
                    if (cfg_type == 2) out_probe->notify_off_multiplier = multiplier;
                }
                if (next == 0 || next == capability) break;
                capability = next;
            }
            out_probe->vendor_id = vendor;
            out_probe->device_id = product;
            /* Common + notify are the minimum read-only proof that the
             * modern virtio PCI transport is advertised.  Device config is
             * recorded separately because some QEMU revisions omit it from
             * the capability chain until the device is fully realized. */
            out_probe->modern_caps = (out_probe->common_cfg != 0 &&
                                      out_probe->notify_cfg != 0);
            out_probe->modern_mapping_safe =
                out_probe->modern_caps &&
                vm_map_mmio_identity(out_probe->common_cfg, 0x1000) == AGENT_OS_OK &&
                vm_map_mmio_identity(out_probe->notify_cfg, 0x1000) == AGENT_OS_OK;
            return out_probe->modern_caps;
        }
    }
    return 0;
}

static int configure_input_modern(AgentOsVirtioProbe *probe) {
    if (probe == 0 || !probe->modern_mapping_safe || probe->common_cfg == 0 ||
        probe->notify_cfg == 0 || probe->notify_off_multiplier == 0) return 0;
    volatile uint8_t *common = (volatile uint8_t *)(uintptr_t)probe->common_cfg;
    volatile uint8_t *notify = (volatile uint8_t *)(uintptr_t)probe->notify_cfg;
    /* The status transition is intentionally conservative: accept no device
     * features, then require FEATURES_OK before touching the queue. */
    common[0x14] = 0;
    common[0x14] = 1 | 2;
    *(volatile uint32_t *)(void *)(common + 0x08) = 0;
    (void)*(volatile uint32_t *)(const void *)(common + 0x04);
    *(volatile uint32_t *)(void *)(common + 0x0c) = 0;
    common[0x14] = 1 | 2 | 8;
    if ((common[0x14] & 8) == 0) return 0;
    *(volatile uint16_t *)(void *)(common + 0x16) = 0;
    uint16_t queue_size = *(volatile uint16_t *)(const void *)(common + 0x18);
    if (queue_size == 0) return 0;
    if (queue_size > 256) queue_size = 256;
    for (uint32_t index = 0; index < sizeof(input_queue_page); ++index) input_queue_page[index] = 0;
    for (uint32_t index = 0; index < sizeof(input_event_page); ++index) input_event_page[index] = 0;
    VirtioDescriptor *descriptors = (VirtioDescriptor *)(void *)input_queue_page;
    uint8_t *avail = input_queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
    uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                            4u + (uint32_t)queue_size * 2u + 0xfffu) & ~0xfffu;
    if (used_offset + 4u + (uint32_t)queue_size * 8u > sizeof(input_queue_page)) return 0;
    uint8_t *used = input_queue_page + used_offset;
    descriptors[0].address = (uint64_t)(uintptr_t)input_event_page;
    descriptors[0].length = sizeof(AgentOsVirtioInputEvent);
    descriptors[0].flags = 2;
    *(volatile uint16_t *)(void *)(common + 0x18) = queue_size;
    *(volatile uint64_t *)(void *)(common + 0x20) = (uint64_t)(uintptr_t)input_queue_page;
    *(volatile uint64_t *)(void *)(common + 0x28) = (uint64_t)(uintptr_t)avail;
    *(volatile uint64_t *)(void *)(common + 0x30) = (uint64_t)(uintptr_t)used;
    probe->notify_off = *(volatile uint16_t *)(const void *)(common + 0x1e);
    volatile uint16_t *notify_queue = (volatile uint16_t *)(uintptr_t)(
        probe->notify_cfg + (uint64_t)probe->notify_off * probe->notify_off_multiplier);
    if (vm_map_mmio_identity((uint64_t)(uintptr_t)notify_queue, sizeof(*notify_queue)) != AGENT_OS_OK) return 0;
    *(volatile uint16_t *)(void *)(common + 0x1c) = 1;
    *(volatile uint16_t *)(void *)(notify_queue) = 0;
    probe->queue_size = queue_size;
    probe->queue_ready = 1;
    return 1;
}

static uint32_t pci_bar0(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read32(bus, device, function, 0x10);
}

static int probe_products(AgentOsVirtioProbe *out_probe,
                          uint16_t product_a, uint16_t product_b,
                          uint16_t product_c, int configure_tx_queue) {
    if (out_probe == 0) return 0;
    *out_probe = (AgentOsVirtioProbe){0};
    for (uint16_t bus = 0; bus < 32; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            uint32_t id = pci_config_read32((uint8_t)bus, device, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xffffu);
            uint16_t product = (uint16_t)(id >> 16);
            if (vendor != 0x1af4 ||
                (product != product_a && product != product_b &&
                 product != product_c)) {
                continue;
            }
            uint32_t bar = pci_bar0((uint8_t)bus, device, 0);
            if ((bar & 1u) == 0 || (bar & 0xffffu) == 0xffffu) continue;
            uint16_t io = (uint16_t)(bar & ~UINT32_C(3));
            uint8_t status = inb((uint16_t)(io + 0x12));
            outb((uint16_t)(io + 0x12), 0);
            outb((uint16_t)(io + 0x12), 1); /* ACKNOWLEDGE */
            outb((uint16_t)(io + 0x12), 3); /* DRIVER */
            (void)inl((uint16_t)(io + 0x00)); /* feature read, accept none */
            outl((uint16_t)(io + 0x04), 0);
            outb((uint16_t)(io + 0x12), 0x0b); /* FEATURES_OK */
            if ((inb((uint16_t)(io + 0x12)) & 0x08u) == 0) continue;
            outw((uint16_t)(io + 0x0e), 0); /* queue 0 */
            uint16_t queue_size = inw((uint16_t)(io + 0x0c));
            if (queue_size == 0 || queue_size > 256) continue;
            for (uint32_t index = 0; index < sizeof(queue_page); ++index) {
                queue_page[index] = 0;
            }
            outl((uint16_t)(io + 0x08),
                 (uint32_t)((uint64_t)(uintptr_t)queue_page >> 12));
            uint16_t tx_queue_size = 0;
            if (configure_tx_queue) {
                outw((uint16_t)(io + 0x0e), 1); /* queue 1: transmit */
                tx_queue_size = inw((uint16_t)(io + 0x0c));
                if (tx_queue_size == 0 || tx_queue_size > 256) continue;
                for (uint32_t index = 0; index < sizeof(net_tx_queue_page); ++index) {
                    net_tx_queue_page[index] = 0;
                }
                outl((uint16_t)(io + 0x08),
                     (uint32_t)((uint64_t)(uintptr_t)net_tx_queue_page >> 12));
            }
            outb((uint16_t)(io + 0x12), 0x0f); /* DRIVER_OK */
            if ((inb((uint16_t)(io + 0x12)) & 0x04u) == 0) continue;
            out_probe->vendor_id = vendor;
            out_probe->device_id = product;
            out_probe->io_base = io;
            out_probe->queue_size = queue_size;
            out_probe->tx_queue_size = tx_queue_size;
            out_probe->queue_ready = 1;
            (void)status;
            return 1;
        }
    }
    return 0;
}

static int discover_products(AgentOsVirtioProbe *out_probe,
                             uint16_t product_a, uint16_t product_b,
                             uint16_t product_c) {
    if (out_probe == 0) return 0;
    *out_probe = (AgentOsVirtioProbe){0};
    for (uint16_t bus = 0; bus < 32; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            uint32_t id = pci_config_read32((uint8_t)bus, device, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xffffu);
            uint16_t product = (uint16_t)(id >> 16);
            if (vendor != 0x1af4 ||
                (product != product_a && product != product_b &&
                 product != product_c)) continue;
            out_probe->vendor_id = vendor;
            out_probe->device_id = product;
            return 1;
        }
    }
    return 0;
}

int agent_os_virtio_probe_block(AgentOsVirtioProbe *out_probe) {
    return probe_products(out_probe, 0x1001, 0x1042, 0x1045, 0);
}

int agent_os_virtio_block_read_sector0(const AgentOsVirtioProbe *probe) {
    if (probe == 0 || !probe->queue_ready || probe->io_base == 0 ||
        probe->queue_size == 0) return 0;
    const uint16_t io = probe->io_base;
    VirtioDescriptor *descriptors = (VirtioDescriptor *)(void *)queue_page;
    uint16_t queue_size = probe->queue_size;
    uint8_t *avail = queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
    uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                            4u + (uint32_t)queue_size * 2u + 0xfffu) &
                           ~0xfffu;
    uint8_t *used = queue_page + used_offset;
    VirtioBlkRequest *request = (VirtioBlkRequest *)(void *)request_page;
    for (uint32_t index = 0; index < sizeof(queue_page); ++index) queue_page[index] = 0;
    for (uint32_t index = 0; index < sizeof(request_page); ++index) request_page[index] = 0;
    descriptors[0].address = (uint64_t)(uintptr_t)request;
    descriptors[0].length = 16;
    descriptors[0].flags = 1; /* NEXT */
    descriptors[0].next = 1;
    descriptors[1].address = (uint64_t)(uintptr_t)request->data;
    descriptors[1].length = sizeof(request->data);
    descriptors[1].flags = 3; /* NEXT | DEVICE_WRITE */
    descriptors[1].next = 2;
    descriptors[2].address = (uint64_t)(uintptr_t)&request->status;
    descriptors[2].length = 1;
    descriptors[2].flags = 2; /* DEVICE_WRITE */
    request->type = 0; /* VIRTIO_BLK_T_IN */
    request->sector = 0;
    uint16_t *avail_index = (uint16_t *)(void *)(avail + 2);
    uint16_t *avail_ring = (uint16_t *)(void *)(avail + 4);
    *avail_index = 1;
    avail_ring[0] = 0;
    __asm__ volatile ("mfence" : : : "memory");
    outw((uint16_t)(io + 0x10), 0);
    uint16_t *used_index = (uint16_t *)(void *)(used + 2);
    for (uint32_t spin = 0; spin < 10000000u; ++spin) {
        if (*used_index == 1) {
            __asm__ volatile ("mfence" : : : "memory");
            return request->status == 0 && request->data[510] == 0x55 &&
                   request->data[511] == 0xaa;
        }
    }
    return 0;
}

int agent_os_virtio_probe_net(AgentOsVirtioProbe *out_probe) {
    return probe_products(out_probe, 0x1000, 0x1041, 0x1044, 1);
}

int agent_os_virtio_net_send_test_packet(const AgentOsVirtioProbe *probe) {
    if (probe == 0 || !probe->queue_ready || probe->io_base == 0 ||
        probe->tx_queue_size == 0) return 0;
    const uint16_t io = probe->io_base;
    const uint16_t queue_size = probe->tx_queue_size;
    VirtioDescriptor *descriptors = (VirtioDescriptor *)(void *)net_tx_queue_page;
    uint8_t *avail = net_tx_queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
    uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                            4u + (uint32_t)queue_size * 2u + 0xfffu) & ~0xfffu;
    uint8_t *used = net_tx_queue_page + used_offset;
    for (uint32_t index = 0; index < sizeof(net_tx_queue_page); ++index) net_tx_queue_page[index] = 0;
    for (uint32_t index = 0; index < sizeof(net_tx_page); ++index) net_tx_page[index] = 0;
    /* Legacy virtio-net requires a 10-byte header before each Ethernet frame. */
    uint8_t *frame = net_tx_page + 10;
    const uint8_t packet[60] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x52, 0x54, 0x00, 0x12, 0x34, 0x56,
        0x88, 0xb5, 'A', 'G', 'E', 'N', 'T', 'O', 'S', '-', 'G', '6',
    };
    for (uint32_t index = 0; index < sizeof(packet); ++index) frame[index] = packet[index];
    descriptors[0].address = (uint64_t)(uintptr_t)net_tx_page;
    descriptors[0].length = 10;
    descriptors[0].flags = 1; /* NEXT */
    descriptors[0].next = 1;
    descriptors[1].address = (uint64_t)(uintptr_t)frame;
    descriptors[1].length = sizeof(packet);
    descriptors[1].flags = 0; /* device reads the frame */
    uint16_t *avail_index = (uint16_t *)(void *)(avail + 2);
    uint16_t *avail_ring = (uint16_t *)(void *)(avail + 4);
    *avail_index = 1;
    avail_ring[0] = 0;
    __asm__ volatile ("mfence" : : : "memory");
    outw((uint16_t)(io + 0x10), 1); /* notify queue 1 */
    uint16_t *used_index = (uint16_t *)(void *)(used + 2);
    for (uint32_t spin = 0; spin < 10000000u; ++spin) {
        if (*used_index == 1) {
            __asm__ volatile ("mfence" : : : "memory");
            return 1;
        }
    }
    return 0;
}

int agent_os_virtio_net_receive_test_packet(const AgentOsVirtioProbe *probe) {
    if (probe == 0 || !probe->queue_ready || probe->io_base == 0 ||
        probe->queue_size == 0) return -1;
    const uint16_t io = probe->io_base;
    const uint16_t queue_size = probe->queue_size;
    VirtioDescriptor *descriptors = (VirtioDescriptor *)(void *)queue_page;
    uint8_t *avail = queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
    uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                            4u + (uint32_t)queue_size * 2u + 0xfffu) & ~0xfffu;
    if (used_offset + 4u + (uint32_t)queue_size * 8u > sizeof(queue_page)) return -1;
    uint8_t *used = queue_page + used_offset;
    for (uint32_t index = 0; index < sizeof(queue_page); ++index) queue_page[index] = 0;
    for (uint32_t index = 0; index < sizeof(net_tx_page); ++index) net_tx_page[index] = 0;
    descriptors[0].address = (uint64_t)(uintptr_t)net_tx_page;
    descriptors[0].length = sizeof(net_tx_page);
    descriptors[0].flags = 2; /* DEVICE_WRITE */
    uint16_t *avail_index = (uint16_t *)(void *)(avail + 2);
    uint16_t *avail_ring = (uint16_t *)(void *)(avail + 4);
    uint16_t *used_index = (uint16_t *)(void *)(used + 2);
    avail_ring[0] = 0;
    *avail_index = 1;
    __asm__ volatile ("mfence" : : : "memory");
    outw((uint16_t)(io + 0x10), 0); /* receive queue */
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        if (*used_index == 1) {
            __asm__ volatile ("mfence" : : : "memory");
            return 1;
        }
    }
    return 0;
}

int agent_os_virtio_probe_input(AgentOsVirtioProbe *out_probe) {
    if (probe_products(out_probe, 0x1052, 0x1053, 0x1054, 0)) return 1;
    /* Modern virtio-input devices expose MMIO common configuration rather
     * than the legacy I/O BAR.  Discovery and the bounded queue setup happen
     * before any event completion is claimed. */
    if (discover_input_modern(out_probe)) {
        (void)configure_input_modern(out_probe);
        return 1;
    }
    return discover_products(out_probe, 0x1052, 0x1053, 0x1054);
}

int agent_os_virtio_input_read_event(const AgentOsVirtioProbe *probe,
                                     AgentOsVirtioInputEvent *out_event) {
    if (probe != 0 && out_event != 0 && probe->modern_caps &&
        probe->queue_ready && probe->common_cfg != 0 && probe->notify_cfg != 0) {
        uint16_t queue_size = probe->queue_size;
        if (queue_size == 0 || queue_size > 256) return -1;
        uint8_t *avail = input_queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
        uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                                4u + (uint32_t)queue_size * 2u + 0xfffu) & ~0xfffu;
        if (used_offset + 4u + (uint32_t)queue_size * 8u > sizeof(input_queue_page)) return -1;
        uint8_t *used = input_queue_page + used_offset;
        uint16_t *avail_index = (uint16_t *)(void *)(avail + 2);
        uint16_t *avail_ring = (uint16_t *)(void *)(avail + 4);
        uint16_t *used_index = (uint16_t *)(void *)(used + 2);
        *avail_index = 1;
        avail_ring[0] = 0;
        __asm__ volatile ("mfence" : : : "memory");
        volatile uint16_t *notify_queue = (volatile uint16_t *)(uintptr_t)(
            probe->notify_cfg + (uint64_t)probe->notify_off * probe->notify_off_multiplier);
        *notify_queue = 0;
        for (uint32_t spin = 0; spin < 100000000u; ++spin) {
            if (*used_index == 1) {
                __asm__ volatile ("mfence" : : : "memory");
                *out_event = *(const AgentOsVirtioInputEvent *)(const void *)input_event_page;
                return 1;
            }
        }
        return 0;
    }
    if (probe == 0 || out_event == 0 || !probe->queue_ready ||
        probe->io_base == 0 || probe->queue_size == 0 ||
        probe->queue_size > 256) return -1;
    const uint16_t io = probe->io_base;
    const uint16_t queue_size = probe->queue_size;
    VirtioDescriptor *descriptors = (VirtioDescriptor *)(void *)input_queue_page;
    uint8_t *avail = input_queue_page + (uint32_t)queue_size * sizeof(VirtioDescriptor);
    uint32_t used_offset = ((uint32_t)queue_size * sizeof(VirtioDescriptor) +
                            4u + (uint32_t)queue_size * 2u + 0xfffu) & ~0xfffu;
    if (used_offset + 4u + (uint32_t)queue_size * 8u > sizeof(input_queue_page)) return -1;
    uint8_t *used = input_queue_page + used_offset;
    for (uint32_t index = 0; index < sizeof(input_queue_page); ++index) input_queue_page[index] = 0;
    /* Reuse the bounded scratch page; the input fixture is run independently
     * of the net TX fixture and must not grow the identity-mapped BSS. */
    for (uint32_t index = 0; index < sizeof(input_event_page); ++index) input_event_page[index] = 0;
    descriptors[0].address = (uint64_t)(uintptr_t)input_event_page;
    descriptors[0].length = sizeof(AgentOsVirtioInputEvent);
    descriptors[0].flags = 2; /* DEVICE_WRITE */
    uint16_t *avail_index = (uint16_t *)(void *)(avail + 2);
    uint16_t *avail_ring = (uint16_t *)(void *)(avail + 4);
    uint16_t *used_index = (uint16_t *)(void *)(used + 2);
    avail_ring[0] = 0;
    *avail_index = 1;
    __asm__ volatile ("mfence" : : : "memory");
    outw((uint16_t)(io + 0x10), 0);
    for (uint32_t spin = 0; spin < 100000000u; ++spin) {
        if (*used_index == 1) {
            __asm__ volatile ("mfence" : : : "memory");
            *out_event = *(const AgentOsVirtioInputEvent *)(const void *)input_event_page;
            return 1;
        }
    }
    /* A real host event is required for completion.  Leaving this as WAIT is
     * deliberate: no synthetic event is manufactured when QEMU has no input. */
    return 0;
}

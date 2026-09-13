#include "boot_info.h"
#include <stddef.h>
#include "interrupts.h"
#include "vm.h"
#include "physmem.h"
#include "syscall.h"
#include "process.h"
#include "capability_table.h"
#include "ipc_endpoint.h"
#include "elf_user_loader.h"
#include "virtio_pci.h"
#include "policy_token.h"
#include "initrd.h"

static IdtEntry idt[256] __attribute__((aligned(16)));
extern const uintptr_t isr_stub_table[256];
extern void isr_syscall(void);
extern void enter_user_mode(uint64_t entry_rip, uint64_t user_rsp);
extern uint64_t high_half_rip_probe(void);
extern uint8_t user_entry_secondary[];
#if defined(AGENT_OS_TEST_IPC_BLOCKING_MULTI) || \
    defined(AGENT_OS_TEST_GROUP_RECURSIVE)
extern uint8_t user_entry_tertiary[];
#endif
extern uint8_t user_entry[];
extern uint8_t kernel_stack_top[];

static Tss64 tss __attribute__((aligned(16)));
static uint64_t kernel_gdt[8] __attribute__((aligned(8)));
static AgentOsProcessTable process_table;
static AgentOsProcessId bootstrap_process;
static AgentOsProcessId secondary_process;
#if defined(AGENT_OS_TEST_IPC_BLOCKING_MULTI) || \
    defined(AGENT_OS_TEST_GROUP_RECURSIVE)
static AgentOsProcessId tertiary_process;
#define AGENT_OS_BOOTSTRAP_PROCESS_COUNT 3u
#else
#define AGENT_OS_BOOTSTRAP_PROCESS_COUNT 2u
#endif
static AgentOsAddressSpace process_spaces[AGENT_OS_BOOTSTRAP_PROCESS_COUNT];
#define AGENT_OS_IPC_ENDPOINT_POOL_SIZE 4u
/* Slot zero is the compatibility bootstrap endpoint.  The remaining slots
 * are Ring-0-owned dynamic objects; no user pointer is ever used as storage. */
static AgentOsIpcEndpoint ipc_endpoints[AGENT_OS_IPC_ENDPOINT_POOL_SIZE];
#define ipc_endpoint (ipc_endpoints[0])
static AgentOsPolicyTokenTable policy_tokens;
static uint8_t policy_authority;
static uint64_t policy_token_clock;
static uint64_t ipc_wait_sequence;
/* Emergency state is a kernel-enforced gate, not a Ring 3 convention.  The
 * Policy Firewall may request it through SYS_POLICY_PAUSE/RESUME only after
 * proving possession of the policy authority capability. */
static int policy_emergency_paused;
static int ipc_ready;
static uint8_t user_load_storage[UINT64_C(0x10000)]
    __attribute__((aligned(4096)));
static uint8_t shared_test_page[4096] __attribute__((aligned(4096)));
extern uint8_t user_image_start[];
extern uint8_t user_image_end[];
extern char __kernel_text_start[];
typedef struct __attribute__((packed)) Gdtr {
    uint16_t limit;
    uint64_t base;
} Gdtr;
extern void load_kernel_gdt(const Gdtr *gdtr);
static void serial_print(const char *text);
static void serial_hex(uint64_t value);
static int user_range_ok(uint64_t address, uint64_t length, int write);
static int copy_ipc_to_process(AgentOsProcess *process, uint64_t address,
                               const IpcMessage *message);

static AgentOsIpcEndpoint *ipc_endpoint_from_object(uint64_t object) {
    for (uint32_t index = 0; index < AGENT_OS_IPC_ENDPOINT_POOL_SIZE; ++index) {
        AgentOsIpcEndpoint *endpoint = &ipc_endpoints[index];
        if (endpoint->active && object == (uint64_t)(uintptr_t)endpoint) {
            return endpoint;
        }
    }
    return 0;
}

static void clear_blocked_ipc(AgentOsProcess *process, uint64_t result) {
    if (process == 0) return;
    process->frame.rax = result;
    process->blocked_ipc_buffer = 0;
    process->blocked_ipc_sequence = 0;
    process->blocked_ipc_endpoint = 0;
    process->blocked_ipc_capability = 0;
    process->blocked_ipc_pending = 0;
    process->state = AGENT_OS_PROCESS_READY;
}

static void wake_ipc_waiters(AgentOsIpcEndpoint *endpoint, uint64_t result) {
    if (endpoint == 0) return;
    for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
        AgentOsProcess *process = &process_table.entries[index];
        if (process->state == AGENT_OS_PROCESS_BLOCKED &&
            process->blocked_ipc_endpoint == (uint64_t)(uintptr_t)endpoint) {
            clear_blocked_ipc(process, result);
        }
    }
}

static int bytes_contain(const uint8_t *bytes, uint32_t length,
                         const char *needle) {
    uint32_t needle_length = 0;
    while (needle[needle_length] != '\0') {
        ++needle_length;
    }
    if (needle_length == 0 || needle_length > length) return 0;
    for (uint32_t offset = 0; offset + needle_length <= length; ++offset) {
        uint32_t index = 0;
        while (index < needle_length &&
               bytes[offset + index] == (uint8_t)needle[index]) {
            ++index;
        }
        if (index == needle_length) return 1;
    }
    return 0;
}

/* Validate the bounded initrd envelope before Ring 3 receives its service
 * image.  This is intentionally a container check rather than a filesystem:
 * the manifest remains Supervisor input, while the kernel only proves ranges,
 * flags and the service ELF envelope. */
static int initrd_service_image(const BootInfo *boot_info,
                                const uint8_t **out_image,
                                uint64_t *out_size) {
    if (boot_info == 0 || out_image == 0 || out_size == 0 ||
        (boot_info->flags & AGENT_OS_BOOT_V2_FLAG_INITRD) == 0) {
        return 0;
    }
    if (boot_info->initrd_phys_start == 0 ||
        boot_info->initrd_phys_end <= boot_info->initrd_phys_start ||
        boot_info->initrd_phys_end - boot_info->initrd_phys_start >
            AGENT_OS_INITRD_MAX_SIZE ||
        boot_info->initrd_phys_end > AGENT_OS_IDENTITY_MAP_END) {
        return 0;
    }
    uint64_t total64 = boot_info->initrd_phys_end -
                       boot_info->initrd_phys_start;
    if (total64 > UINT32_MAX) return 0;
    const uint8_t *base = (const uint8_t *)(uintptr_t)boot_info->initrd_phys_start;
    const AgentOsInitrdHeader *header = (const AgentOsInitrdHeader *)base;
    uint32_t total = (uint32_t)total64;
    if (header->magic != AGENT_OS_INITRD_MAGIC ||
        header->version != AGENT_OS_INITRD_VERSION ||
        header->header_size != AGENT_OS_INITRD_HEADER_SIZE ||
        header->total_size != total ||
        header->service_count == 0 ||
        header->service_count > AGENT_OS_INITRD_MAX_SERVICES ||
        (header->flags & (AGENT_OS_INITRD_FLAG_MANIFEST_JSON |
                          AGENT_OS_INITRD_FLAG_SERVICE_ELF)) !=
            (AGENT_OS_INITRD_FLAG_MANIFEST_JSON |
             AGENT_OS_INITRD_FLAG_SERVICE_ELF) ||
        !agent_os_initrd_range_ok(header->manifest_offset,
                                  header->manifest_size, total) ||
        !agent_os_initrd_range_ok(header->service_offset,
                                  header->service_size, total) ||
        header->manifest_offset < header->header_size ||
        header->service_offset < header->manifest_offset +
                                  header->manifest_size ||
        header->service_size < header->service_count *
                                   AGENT_OS_INITRD_SERVICE_ENTRY_SIZE ||
        header->service_size == 0 || header->manifest_size == 0) {
        return 0;
    }
    const uint8_t *manifest = base + header->manifest_offset;
    if ((!bytes_contain(manifest, header->manifest_size, "\"service_id\"") &&
         !bytes_contain(manifest, header->manifest_size, "\"services\"")) ||
        !bytes_contain(manifest, header->manifest_size, "\"entrypoint\"") ||
        !bytes_contain(manifest, header->manifest_size, "\"heartbeat\"") ||
        !bytes_contain(manifest, header->manifest_size, "\"restart\"")) {
        return 0;
    }
    const uint32_t table_bytes = header->service_count *
                                 AGENT_OS_INITRD_SERVICE_ENTRY_SIZE;
    const AgentOsInitrdServiceEntry *entries =
        (const AgentOsInitrdServiceEntry *)(base + header->service_offset);
    for (uint32_t index = 0; index < header->service_count; ++index) {
        const AgentOsInitrdServiceEntry *entry = &entries[index];
        if (entry->reserved != 0 || entry->manifest_index >= header->service_count ||
            entry->image_size == 0 ||
            !agent_os_initrd_range_ok(entry->image_offset, entry->image_size,
                                      total) ||
            entry->image_offset < header->service_offset + table_bytes ||
            entry->image_offset + entry->image_size >
                header->service_offset + header->service_size) {
            return 0;
        }
    }
    /* Keep the manifest/range checks above local to the Supervisor handoff,
     * then use the shared bounded selector for the actual image pointer. The
     * current boot slice intentionally selects service 0; later Supervisor
     * policy may pass another validated index. */
    return agent_os_initrd_select_service(base, total, 0, out_image, out_size) ==
           AGENT_OS_OK;
}

static void scheduler_halt(const char *reason) {
    serial_print(reason);
    serial_print("\r\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

/* Replace the interrupted syscall frame with the next READY process.  The
 * interrupt entry layout is intentionally identical to SyscallFrame, so the
 * assembly epilogue restores this process without a privileged trampoline. */
static int schedule_into_next(SyscallFrame *frame) {
    AgentOsProcessId next_id;
    if (agent_os_process_schedule(&process_table, &next_id) !=
        AGENT_OS_PROCESS_OK) {
        return 0;
    }
    AgentOsProcess *next_process;
    if (agent_os_process_lookup(&process_table, next_id, &next_process) !=
        AGENT_OS_PROCESS_OK || next_process->address_space_root == 0) {
        return 0;
    }
    vm_load_address_space(next_process->address_space_root);
    SyscallFrame next_frame;
    if (agent_os_process_get_frame(&process_table, next_id, &next_frame) !=
        AGENT_OS_PROCESS_OK) {
        return 0;
    }
    serial_print("G2 CR3 switch OK\r\n");
    *frame = next_frame;
    return 1;
}

#if defined(AGENT_OS_TEST_TIMER_PREEMPT)
static void exception_to_syscall(const ExceptionFrame *source,
                                 SyscallFrame *target) {
    target->r15 = source->r15; target->r14 = source->r14;
    target->r13 = source->r13; target->r12 = source->r12;
    target->r11 = source->r11; target->r10 = source->r10;
    target->r9 = source->r9; target->r8 = source->r8;
    target->rdi = source->rdi; target->rsi = source->rsi;
    target->rbp = source->rbp; target->rbx = source->rbx;
    target->rdx = source->rdx; target->rcx = source->rcx;
    target->rax = source->rax; target->rip = source->rip;
    target->cs = source->cs; target->rflags = source->rflags;
    target->rsp = source->rsp; target->ss = source->ss;
}

static void syscall_to_exception(const SyscallFrame *source,
                                 ExceptionFrame *target) {
    target->r15 = source->r15; target->r14 = source->r14;
    target->r13 = source->r13; target->r12 = source->r12;
    target->r11 = source->r11; target->r10 = source->r10;
    target->r9 = source->r9; target->r8 = source->r8;
    target->rdi = source->rdi; target->rsi = source->rsi;
    target->rbp = source->rbp; target->rbx = source->rbx;
    target->rdx = source->rdx; target->rcx = source->rcx;
    target->rax = source->rax; target->rip = source->rip;
    target->cs = source->cs; target->rflags = source->rflags;
    target->rsp = source->rsp; target->ss = source->ss;
}

static int preempt_user_timer(ExceptionFrame *frame) {
    AgentOsProcessId current_id = agent_os_process_current(&process_table);
    SyscallFrame saved;
    if (current_id == AGENT_OS_PROCESS_INVALID) return 0;
    exception_to_syscall(frame, &saved);
    if (agent_os_process_set_frame(&process_table, current_id, &saved) !=
        AGENT_OS_PROCESS_OK) return 0;
    SyscallFrame next = saved;
    if (!schedule_into_next(&next)) return 0;
    syscall_to_exception(&next, frame);
    serial_print("TIMER PREEMPT switched task\r\n");
    return 1;
}
#endif

/* Page faults from a user task are lifecycle events at this stage of the
 * kernel: the faulting task becomes a zombie and the next READY task gets
 * its saved frame.  This helper deliberately reuses the same scheduler and
 * CR3 switch used by SYS_EXIT, so fault recovery cannot bypass process or
 * address-space bookkeeping. */
static int recover_user_exception(ExceptionFrame *frame) {
    AgentOsProcessId current_id = agent_os_process_current(&process_table);
    if (current_id == AGENT_OS_PROCESS_INVALID ||
        agent_os_process_exit(&process_table, current_id, -14) !=
            AGENT_OS_PROCESS_OK) {
        return 0;
    }

    SyscallFrame next_frame;
    AgentOsProcessId next_id;
    if (agent_os_process_schedule(&process_table, &next_id) !=
            AGENT_OS_PROCESS_OK ||
        agent_os_process_get_frame(&process_table, next_id, &next_frame) !=
            AGENT_OS_PROCESS_OK) {
        return 0;
    }
    AgentOsProcess *next_process;
    if (agent_os_process_lookup(&process_table, next_id, &next_process) !=
            AGENT_OS_PROCESS_OK || next_process->address_space_root == 0) {
        return 0;
    }

    vm_load_address_space(next_process->address_space_root);
    frame->r15 = next_frame.r15;
    frame->r14 = next_frame.r14;
    frame->r13 = next_frame.r13;
    frame->r12 = next_frame.r12;
    frame->r11 = next_frame.r11;
    frame->r10 = next_frame.r10;
    frame->r9 = next_frame.r9;
    frame->r8 = next_frame.r8;
    frame->rdi = next_frame.rdi;
    frame->rsi = next_frame.rsi;
    frame->rbp = next_frame.rbp;
    frame->rbx = next_frame.rbx;
    frame->rdx = next_frame.rdx;
    frame->rcx = next_frame.rcx;
    frame->rax = next_frame.rax;
    frame->rip = next_frame.rip;
    frame->cs = next_frame.cs;
    frame->rflags = next_frame.rflags;
    frame->rsp = next_frame.rsp;
    frame->ss = next_frame.ss;
    return 1;
}

static void tss_init(void) {
    for (uint32_t index = 0; index < sizeof(tss); ++index) {
        ((uint8_t *)&tss)[index] = 0;
    }
    tss.rsp0 = (uint64_t)(uintptr_t)kernel_stack_top;
    tss.iomap_base = sizeof(tss);

    /* Mirror the bootstrap selectors, then install the TSS descriptor in
     * memory owned by the kernel.  No BootInfo field points into stage2. */
    kernel_gdt[0] = UINT64_C(0);
    kernel_gdt[1] = UINT64_C(0x00CF9A000000FFFF); /* kernel 32-bit code */
    kernel_gdt[2] = UINT64_C(0x00CF92000000FFFF); /* kernel data */
    kernel_gdt[3] = UINT64_C(0x00AF9A000000FFFF); /* kernel 64-bit code */
    kernel_gdt[4] = UINT64_C(0x00CFF2000000FFFF); /* user data */
    kernel_gdt[5] = UINT64_C(0x00AFFA000000FFFF); /* user 64-bit code */

    uint64_t base = (uint64_t)(uintptr_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    kernel_gdt[6] = limit |
        ((base & UINT64_C(0xFFFFFF)) << 16) |
        (UINT64_C(0x89) << 40) |
        (((base >> 24) & UINT64_C(0xFF)) << 56);
    kernel_gdt[7] = base >> 32;

    Gdtr gdtr = {
        .limit = (uint16_t)(sizeof(kernel_gdt) - 1),
        .base = (uint64_t)(uintptr_t)&kernel_gdt[0],
    };
    load_kernel_gdt(&gdtr);

    uint16_t selector = 0x30;
    __asm__ volatile ("movw %0, %%ax\n\t.byte 0x66, 0x0f, 0x00, 0xd8"
                      : : "rm"(selector) : "ax");
}

static inline void lidt(const Idtr *idtr) {
    __asm__ volatile ("lidt %0" : : "m"(*idtr));
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* SeaBIOS leaves the legacy PIC enabled. Until the kernel owns and remaps
 * IRQ vectors, mask both controllers so IRQ0 cannot be mistaken for #DF
 * vector 8 by the exception-only stubs. */
static void pic_mask_all(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

#if defined(AGENT_OS_TEST_TIMER_PREEMPT)
static void pic_timer_init(void) {
    /* Remap IRQ0 to vector 32 and unmask only the timer line. */
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFE); outb(0xA1, 0xFF);
    /* PIT channel 0, square wave, divisor 65535 (~18.2 Hz). */
    outb(0x43, 0x34);
    outb(0x40, 0xFF); outb(0x40, 0xFF);
}
#endif

static void serial_write(char value) {
    while ((inb(0x3FD) & 0x20) == 0) {
    }
    outb(0x3F8, (uint8_t)value);
}

static void serial_print(const char *text) {
    while (*text != '\0') {
        serial_write(*text++);
    }
}

static void serial_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    serial_print("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write(digits[(value >> shift) & 0xF]);
    }
}

static void serial_exception_hex(const char *label, uint64_t value) {
    serial_print(label);
    serial_hex(value);
    serial_print("\r\n");
}

static uint32_t count_usable_e820(const BootInfo *boot_info) {
    if (boot_info->memory_map_addr == 0 ||
        boot_info->memory_map_entry_size < sizeof(E820Entry)) {
        return 0;
    }

    const uint8_t *cursor = (const uint8_t *)(uintptr_t)boot_info->memory_map_addr;
    uint32_t usable = 0;
    for (uint32_t index = 0; index < boot_info->memory_map_count; ++index) {
        const E820Entry *entry = (const E820Entry *)(cursor +
            (uint64_t)index * boot_info->memory_map_entry_size);
        if (entry->type == 1 && entry->length != 0) {
            ++usable;
        }
    }
    return usable;
}

void interrupts_init(void) {
    for (uint16_t vector = 0; vector < 256; ++vector) {
        uintptr_t address = isr_stub_table[vector];
        IdtEntry *entry = &idt[vector];
        entry->offset_low = (uint16_t)(address & 0xFFFFu);
        /* Stage 2's 64-bit code descriptor is GDT selector 0x18. */
        entry->selector = 0x18;
        entry->ist = 0;
        entry->type_attr = 0x8E; /* present, ring 0, interrupt gate */
        entry->offset_mid = (uint16_t)((address >> 16) & 0xFFFFu);
        entry->offset_high = (uint32_t)(address >> 32);
        entry->reserved = 0;
    }

    /* User programs enter through the one explicitly DPL3 gate. */
    {
        uintptr_t address = (uintptr_t)&isr_syscall;
        IdtEntry *entry = &idt[0x80];
        entry->offset_low = (uint16_t)(address & 0xFFFFu);
        entry->selector = 0x18;
        entry->ist = 0;
        entry->type_attr = 0xEE; /* present, DPL3, interrupt gate */
        entry->offset_mid = (uint16_t)((address >> 16) & 0xFFFFu);
        entry->offset_high = (uint32_t)(address >> 32);
        entry->reserved = 0;
    }

    Idtr idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uintptr_t)&idt[0],
    };
    lidt(&idtr);
}

static int user_range_ok(uint64_t address, uint64_t length, int write) {
    AgentOsProcessId current = agent_os_process_current(&process_table);
    AgentOsProcess *process;
    if (current != AGENT_OS_PROCESS_INVALID &&
        agent_os_process_lookup(&process_table, current, &process) ==
            AGENT_OS_PROCESS_OK) {
        for (size_t index = 0; index < sizeof(process_spaces) / sizeof(process_spaces[0]); ++index) {
            if (process_spaces[index].root_physical == process->address_space_root) {
                return agent_os_address_space_user_range_ok(
                    &process_spaces[index], address, length, write, 0) == AGENT_OS_OK;
            }
        }
    }
    return vm_user_range_ok(address, length, write);
}

/* Validate a user buffer against a specific process rather than the task
 * currently executing.  Blocking IPC stores the receiver's buffer while a
 * sender is running, so checking only the sender's CR3 would be incorrect. */
static int process_user_range_ok(const AgentOsProcess *process,
                                 uint64_t address, uint64_t length, int write) {
    if (process == 0) {
        return 0;
    }
    for (size_t index = 0;
         index < sizeof(process_spaces) / sizeof(process_spaces[0]); ++index) {
        if (process_spaces[index].root_physical == process->address_space_root) {
            return agent_os_address_space_user_range_ok(
                       &process_spaces[index], address, length, write, 0) ==
                   AGENT_OS_OK;
        }
    }
    return 0;
}

/* The bootstrap process roots share the identity-mapped kernel window and the
 * two fixed user stack pages.  Switch to the receiver root while copying so
 * this helper remains correct when those mappings stop aliasing. */
static int copy_ipc_to_process(AgentOsProcess *process, uint64_t address,
                               const IpcMessage *message) {
    if (process == 0 || message == 0 ||
        !process_user_range_ok(process, address, sizeof(*message), 1)) {
        return 0;
    }
    AgentOsProcessId current_id = agent_os_process_current(&process_table);
    AgentOsProcess *current = 0;
    if (current_id == AGENT_OS_PROCESS_INVALID ||
        agent_os_process_lookup(&process_table, current_id, &current) !=
            AGENT_OS_PROCESS_OK || current->address_space_root == 0 ||
        process->address_space_root == 0) {
        return 0;
    }
    uint64_t current_root = current->address_space_root;
    if (current_root != process->address_space_root) {
        vm_load_address_space(process->address_space_root);
    }
    volatile uint8_t *destination = (volatile uint8_t *)(uintptr_t)address;
    const uint8_t *source = (const uint8_t *)message;
    for (uint64_t index = 0; index < sizeof(*message); ++index) {
        destination[index] = source[index];
    }
    if (current_root != process->address_space_root) {
        vm_load_address_space(current_root);
    }
    return 1;
}

void kernel_syscall_handler(SyscallFrame *frame) {
    AgentOsProcessId current_id = agent_os_process_current(&process_table);
    if (current_id != AGENT_OS_PROCESS_INVALID) {
        /* Save all register state before a yield, exit, or later preemption. */
        (void)agent_os_process_set_frame(&process_table, current_id, frame);
    }
    if (frame->rax == SYS_WRITE) {
        if (!user_range_ok(frame->rdi, frame->rsi, 0)) {
            serial_print("SYSCALL write EFAULT\r\n");
            frame->rax = (uint64_t)-14;
            return;
        }
        const char *text = (const char *)(uintptr_t)frame->rdi;
        uint64_t length = frame->rsi;
        for (uint64_t i = 0; i < length; ++i) {
            serial_write(text[i]);
        }
        serial_print("SYSCALL write OK\r\n");
        frame->rax = length;
        return;
    }
    if (frame->rax == SYS_IPC_CREATE) {
        AgentOsProcess *current_process = 0;
        uint64_t authority_object = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                          &current_process);
        }
        AgentOsStatus authority_status = current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_ADMIN, &authority_object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *created = 0;
        if (authority_status == AGENT_OS_OK &&
            authority_object == (uint64_t)(uintptr_t)&policy_authority &&
            current_process != 0) {
            for (uint32_t endpoint_index = 1;
                 endpoint_index < AGENT_OS_IPC_ENDPOINT_POOL_SIZE;
                 ++endpoint_index) {
                if (!ipc_endpoints[endpoint_index].active) {
                    created = &ipc_endpoints[endpoint_index];
                    break;
                }
            }
        }
        CapabilityHandle created_handle = 0;
        AgentOsStatus status = created != 0
            ? agent_os_ipc_endpoint_create(created)
            : AGENT_OS_E_NO_MEMORY;
        if (status == AGENT_OS_OK) {
            status = agent_os_capability_mint(
                &current_process->capabilities,
                (uint64_t)(uintptr_t)created,
                CAP_RIGHT_SEND | CAP_RIGHT_RECV | CAP_RIGHT_TRANSFER |
                    CAP_RIGHT_REVOKE,
                &created_handle);
            if (status != AGENT_OS_OK) {
                (void)agent_os_ipc_endpoint_destroy(created);
            }
        }
        serial_print(status == AGENT_OS_OK ? "SYSCALL ipc create OK\r\n"
                                            : "SYSCALL ipc create denied\r\n");
        frame->rax = status == AGENT_OS_OK ? created_handle : (uint64_t)-12;
        return;
    }
    if (frame->rax == SYS_IPC_DESTROY) {
        AgentOsProcess *current_process = 0;
        uint64_t object = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                          &current_process);
        }
        AgentOsStatus cap_status = current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_REVOKE, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *endpoint = cap_status == AGENT_OS_OK
            ? ipc_endpoint_from_object(object) : 0;
        AgentOsStatus status = endpoint != 0 && endpoint != &ipc_endpoint
            ? agent_os_ipc_endpoint_destroy(endpoint) : AGENT_OS_E_BAD_CAP;
        if (status == AGENT_OS_OK) {
            wake_ipc_waiters(endpoint, (uint64_t)-125);
            /* Destroy retires every source and transferred sibling handle
             * naming this endpoint before its pool slot can be reused. */
            for (uint32_t process_index = 0;
                 process_index < AGENT_OS_MAX_PROCESSES; ++process_index) {
                AgentOsProcess *owner = &process_table.entries[process_index];
                if (owner->state != AGENT_OS_PROCESS_UNUSED) {
                    (void)agent_os_capability_revoke_object(
                        &owner->capabilities, object);
                }
            }
            status = AGENT_OS_OK;
        }
        serial_print(status == AGENT_OS_OK ? "SYSCALL ipc destroy OK\r\n"
                                            : "SYSCALL ipc destroy denied\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_IPC_CLOSE) {
        AgentOsProcess *current_process = 0;
        uint64_t object = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                          &current_process);
        }
        AgentOsStatus cap_status = current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_REVOKE, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *endpoint = cap_status == AGENT_OS_OK
            ? ipc_endpoint_from_object(object) : 0;
        AgentOsStatus status = endpoint != 0
            ? agent_os_ipc_endpoint_close(endpoint) : AGENT_OS_E_BAD_CAP;
        if (status == AGENT_OS_OK) {
            wake_ipc_waiters(endpoint, (uint64_t)-125);
        }
        serial_print(status == AGENT_OS_OK ? "SYSCALL ipc close OK\r\n"
                                            : "SYSCALL ipc close denied\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_IPC_CALL) {
        uint64_t object;
        AgentOsProcess *current_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus cap_status = ipc_ready
            && current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_SEND, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *endpoint = cap_status == AGENT_OS_OK
            ? ipc_endpoint_from_object(object) : 0;
        if (endpoint == 0) {
            serial_print("SYSCALL ipc send denied\r\n");
            frame->rax = (uint64_t)-13;
            return;
        }
        IpcMessage message = {
            .header = {.version = AGENT_OS_ABI_VERSION, .size = sizeof(IpcMessage)},
            .opcode = (uint32_t)frame->rsi,
            .length = 1,
            .words = {frame->rdx, 0, 0, 0},
        };
        AgentOsStatus status = AGENT_OS_E_NOT_FOUND;
        uint64_t skipped_sequence = 0;
        /* Deliver directly to the oldest blocked receiver.  Scanning the
         * bounded process table is intentional at this stage: it gives every
         * waiter an independent saved frame and buffer without introducing a
         * second unverified scheduler queue. */
        for (;;) {
            AgentOsProcess *waiter = 0;
            for (uint32_t index = 0; index < AGENT_OS_MAX_PROCESSES; ++index) {
                AgentOsProcess *candidate = &process_table.entries[index];
                if (candidate->state != AGENT_OS_PROCESS_BLOCKED ||
                    candidate->blocked_ipc_buffer == 0 ||
                    candidate->blocked_ipc_endpoint !=
                        (uint64_t)(uintptr_t)endpoint ||
                    candidate->blocked_ipc_sequence <= skipped_sequence) {
                    continue;
                }
                if (waiter == 0 || candidate->blocked_ipc_sequence <
                                      waiter->blocked_ipc_sequence) {
                    waiter = candidate;
                }
            }
            if (waiter == 0) break;
            skipped_sequence = waiter->blocked_ipc_sequence;
            uint64_t waiter_object = 0;
            AgentOsStatus waiter_cap_status =
                agent_os_capability_lookup(&waiter->capabilities,
                    waiter->blocked_ipc_capability, CAP_RIGHT_RECV,
                    &waiter_object, 0);
            AgentOsIpcEndpoint *waiter_endpoint =
                waiter_cap_status == AGENT_OS_OK
                    ? ipc_endpoint_from_object(waiter_object) : 0;
            if (waiter_endpoint != endpoint ||
                waiter->blocked_ipc_endpoint != waiter_object) {
                /* A blocked receive does not pin a revoked capability.  Wake
                 * it with a deterministic denial and continue searching for
                 * the next FIFO waiter so the message is not lost. */
                waiter->frame.rax = (uint64_t)-13;
                waiter->blocked_ipc_buffer = 0;
                waiter->blocked_ipc_sequence = 0;
                waiter->blocked_ipc_endpoint = 0;
                waiter->blocked_ipc_capability = 0;
                waiter->blocked_ipc_pending = 0;
                waiter->state = AGENT_OS_PROCESS_READY;
                status = AGENT_OS_E_NOT_FOUND;
                serial_print("SYSCALL ipc wake denied revoked\r\n");
                continue;
            }
            status = copy_ipc_to_process(waiter, waiter->blocked_ipc_buffer,
                                         &message)
                ? AGENT_OS_OK : AGENT_OS_E_FAULT;
            if (status == AGENT_OS_OK) {
                waiter->frame.rax = 0;
                waiter->blocked_ipc_buffer = 0;
                waiter->blocked_ipc_sequence = 0;
                waiter->blocked_ipc_endpoint = 0;
                waiter->blocked_ipc_capability = 0;
                waiter->blocked_ipc_pending = 0;
                waiter->state = AGENT_OS_PROCESS_READY;
                serial_print("SYSCALL ipc send wake OK\r\n");
            } else {
                waiter->frame.rax = (uint64_t)-14;
                waiter->blocked_ipc_buffer = 0;
                waiter->blocked_ipc_sequence = 0;
                waiter->blocked_ipc_endpoint = 0;
                waiter->blocked_ipc_capability = 0;
                waiter->blocked_ipc_pending = 0;
                waiter->state = AGENT_OS_PROCESS_READY;
                status = AGENT_OS_E_NOT_FOUND;
                serial_print("SYSCALL ipc wake denied fault\r\n");
                continue;
            }
            break;
        }
        if (status == AGENT_OS_E_NOT_FOUND) {
            status = agent_os_ipc_send(endpoint, &message);
            serial_print(status == AGENT_OS_OK
                             ? "SYSCALL ipc send OK\r\n"
                             : "SYSCALL ipc send failed\r\n");
        }
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-16;
        return;
    }
    if (frame->rax == SYS_IPC_RECV) {
        uint64_t object;
        AgentOsProcess *current_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus cap_status = ipc_ready && current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_RECV, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *endpoint = cap_status == AGENT_OS_OK
            ? ipc_endpoint_from_object(object) : 0;
        if (endpoint == 0 || !user_range_ok(frame->rsi, sizeof(IpcMessage), 1)) {
            serial_print("SYSCALL ipc recv denied\r\n");
            frame->rax = (uint64_t)-13;
            return;
        }
        IpcMessage message;
        AgentOsStatus status = agent_os_ipc_recv(endpoint, &message);
        if (status == AGENT_OS_OK) {
            volatile uint8_t *destination = (volatile uint8_t *)(uintptr_t)frame->rsi;
            const uint8_t *source = (const uint8_t *)&message;
            for (uint64_t index = 0; index < sizeof(message); ++index) {
                destination[index] = source[index];
            }
            serial_print("SYSCALL ipc recv OK\r\n");
        } else {
            serial_print("SYSCALL ipc recv empty\r\n");
        }
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-11;
        return;
    }
    if (frame->rax == SYS_IPC_RECV_WAIT) {
        uint64_t object;
        AgentOsProcess *current_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus cap_status = ipc_ready && current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_RECV, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsIpcEndpoint *endpoint = cap_status == AGENT_OS_OK
            ? ipc_endpoint_from_object(object) : 0;
        if (endpoint == 0 || !user_range_ok(frame->rsi, sizeof(IpcMessage), 1)) {
            serial_print("SYSCALL ipc recv wait denied\r\n");
            frame->rax = (uint64_t)-13;
            return;
        }
        IpcMessage message;
        AgentOsStatus status = agent_os_ipc_recv(endpoint, &message);
        if (status == AGENT_OS_OK) {
            volatile uint8_t *destination =
                (volatile uint8_t *)(uintptr_t)frame->rsi;
            const uint8_t *source = (const uint8_t *)&message;
            for (uint64_t index = 0; index < sizeof(message); ++index) {
                destination[index] = source[index];
            }
            serial_print("SYSCALL ipc recv wait immediate OK\r\n");
            frame->rax = 0;
            return;
        }
        current_process->blocked_ipc_buffer = frame->rsi;
        current_process->blocked_ipc_sequence = ++ipc_wait_sequence;
        current_process->blocked_ipc_endpoint = object;
        current_process->blocked_ipc_capability = (CapabilityHandle)frame->rdi;
        current_process->blocked_ipc_pending = 0;
        current_process->state = AGENT_OS_PROCESS_BLOCKED;
        frame->rax = 0;
        (void)agent_os_process_set_frame(&process_table, current_id, frame);
        serial_print("SYSCALL ipc recv blocked\r\n");
        if (!schedule_into_next(frame)) {
            scheduler_halt("SYSCALL ipc recv wait scheduler failed");
        }
        return;
    }
    if (frame->rax == SYS_CAP_RESTRICT) {
        AgentOsProcess *current_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        CapabilityHandle restricted = 0;
        AgentOsStatus status = current_process != 0
            ? agent_os_capability_restrict(&current_process->capabilities,
                                           (CapabilityHandle)frame->rdi,
                                           frame->rsi, &restricted)
            : AGENT_OS_E_BAD_CAP;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL cap restrict OK\r\n"
                         : "SYSCALL cap restrict denied\r\n");
        frame->rax = status == AGENT_OS_OK ? restricted : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_CAP_TRANSFER) {
        AgentOsProcess *current_process = 0;
        AgentOsProcess *target_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        int target_status = agent_os_process_lookup(
            &process_table, (AgentOsProcessId)frame->rsi, &target_process);
        CapabilityHandle transferred = 0;
        AgentOsStatus status = current_process != 0 && target_status == AGENT_OS_PROCESS_OK
            ? agent_os_capability_transfer(&current_process->capabilities,
                                           (CapabilityHandle)frame->rdi,
                                           &target_process->capabilities,
                                           frame->rdx, &transferred)
            : AGENT_OS_E_BAD_CAP;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL cap transfer OK\r\n"
                         : "SYSCALL cap transfer denied\r\n");
        frame->rax = status == AGENT_OS_OK ? transferred : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_CAP_REVOKE) {
        AgentOsProcess *current_process = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus status = current_process != 0
            ? agent_os_capability_revoke(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi)
            : AGENT_OS_E_BAD_CAP;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL cap revoke OK\r\n"
                         : "SYSCALL cap revoke denied\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_POLICY_TOKEN_MINT) {
        AgentOsProcess *current_process = 0;
        uint64_t authority_object = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus cap_status = current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_ADMIN, &authority_object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsPolicyTokenHandle token = 0;
        AgentOsProcessId token_owner = frame->r8 == 0
            ? current_id : (AgentOsProcessId)frame->r8;
        AgentOsProcess *owner_process = 0;
        int owner_valid = agent_os_process_lookup(
            &process_table, token_owner, &owner_process) == AGENT_OS_PROCESS_OK;
        uint64_t bound_object = 0;
        AgentOsStatus bound_status = owner_valid
            ? agent_os_capability_lookup(&owner_process->capabilities,
                                         (CapabilityHandle)frame->rsi, 0,
                                         &bound_object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsStatus status = cap_status == AGENT_OS_OK &&
                               authority_object == (uint64_t)(uintptr_t)&policy_authority &&
                               owner_valid && bound_status == AGENT_OS_OK
            ? (policy_emergency_paused
                   ? AGENT_OS_E_BUSY
                   : agent_os_policy_token_mint(&policy_tokens, token_owner,
                                         (CapabilityHandle)frame->rsi,
                                         frame->rdx, frame->r10, &token))
            : AGENT_OS_E_DENIED;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL policy token mint OK\r\n"
                         : status == AGENT_OS_E_BUSY
                             ? "SYSCALL policy token mint paused\r\n"
                             : "SYSCALL policy token mint denied\r\n");
        frame->rax = status == AGENT_OS_OK ? token : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_POLICY_TOKEN_CONSUME) {
        AgentOsProcess *current_process = 0;
        uint64_t bound_object = 0;
        AgentOsStatus bound_status = current_id != AGENT_OS_PROCESS_INVALID &&
            agent_os_process_lookup(&process_table, current_id,
                                    &current_process) == AGENT_OS_PROCESS_OK
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rsi, 0,
                                         &bound_object, 0)
            : AGENT_OS_E_BAD_CAP;
        AgentOsStatus status = bound_status == AGENT_OS_OK &&
                               !policy_emergency_paused
            ? agent_os_policy_token_consume(
                &policy_tokens, (AgentOsPolicyTokenHandle)frame->rdi,
                current_id, (CapabilityHandle)frame->rsi, frame->rdx,
                ++policy_token_clock)
            : policy_emergency_paused && bound_status == AGENT_OS_OK
                ? AGENT_OS_E_BUSY : AGENT_OS_E_DENIED;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL policy token consume OK\r\n"
                         : status == AGENT_OS_E_BUSY
                             ? "SYSCALL policy token consume paused\r\n"
                             : status == AGENT_OS_E_TIMEOUT
                                 ? "SYSCALL policy token consume timeout\r\n"
                                 : "SYSCALL policy token consume denied\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_POLICY_TOKEN_REVOKE) {
        AgentOsStatus status = current_id != AGENT_OS_PROCESS_INVALID
            ? agent_os_policy_token_revoke(
                &policy_tokens, (AgentOsPolicyTokenHandle)frame->rdi,
                current_id)
            : AGENT_OS_E_DENIED;
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL policy token revoke OK\r\n"
                         : "SYSCALL policy token revoke denied\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-13;
        return;
    }
    if (frame->rax == SYS_POLICY_PAUSE || frame->rax == SYS_POLICY_RESUME) {
        AgentOsProcess *current_process = 0;
        uint64_t authority_object = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
        }
        AgentOsStatus status = current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_ADMIN, &authority_object, 0)
            : AGENT_OS_E_BAD_CAP;
        if (status == AGENT_OS_OK &&
            authority_object == (uint64_t)(uintptr_t)&policy_authority) {
            policy_emergency_paused = frame->rax == SYS_POLICY_PAUSE;
            serial_print(policy_emergency_paused
                             ? "SYSCALL policy emergency pause OK\r\n"
                             : "SYSCALL policy emergency resume OK\r\n");
            frame->rax = 0;
        } else {
            serial_print("SYSCALL policy emergency gate denied\r\n");
            frame->rax = (uint64_t)-13;
        }
        return;
    }
    if (frame->rax == SYS_SHM_MAP) {
        uint64_t object;
        AgentOsProcess *current_process = 0;
        AgentOsAddressSpace *current_space = 0;
        if (current_id != AGENT_OS_PROCESS_INVALID) {
            (void)agent_os_process_lookup(&process_table, current_id,
                                           &current_process);
            for (size_t index = 0; index < sizeof(process_spaces) / sizeof(process_spaces[0]); ++index) {
                if (current_process != 0 &&
                    process_spaces[index].root_physical == current_process->address_space_root) {
                    current_space = &process_spaces[index];
                    break;
                }
            }
        }
        AgentOsStatus cap_status = ipc_ready && current_process != 0
            ? agent_os_capability_lookup(&current_process->capabilities,
                                         (CapabilityHandle)frame->rdi,
                                         CAP_RIGHT_MAP, &object, 0)
            : AGENT_OS_E_BAD_CAP;
        if (cap_status != AGENT_OS_OK || object != (uint64_t)(uintptr_t)shared_test_page ||
            current_space == 0 || (frame->rsi & 0xFFF) != 0) {
            serial_print("SYSCALL shm map denied\r\n");
            frame->rax = (uint64_t)-13;
            return;
        }
        AgentOsStatus status = agent_os_address_space_map(
            current_space, frame->rsi, (uint64_t)(uintptr_t)shared_test_page,
            0x1000, AGENT_OS_VM_READ | AGENT_OS_VM_WRITE | AGENT_OS_VM_USER);
        serial_print(status == AGENT_OS_OK
                         ? "SYSCALL shm map OK\r\n"
                         : "SYSCALL shm map failed\r\n");
        frame->rax = status == AGENT_OS_OK ? 0 : (uint64_t)-16;
        return;
    }
    if (frame->rax == SYS_YIELD) {
        if (current_id == AGENT_OS_PROCESS_INVALID ||
            agent_os_process_yield(&process_table) != AGENT_OS_PROCESS_OK ||
            !schedule_into_next(frame)) {
            frame->rax = (uint64_t)-16;
            return;
        }
        serial_print("SYSCALL yield OK - switched task\r\n");
        return;
    }
    if (frame->rax == SYS_WAIT) {
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        int64_t exit_code;
        AgentOsProcessId reaped;
        int status = agent_os_process_wait(&process_table, current_id,
                                           (AgentOsProcessId)frame->rdi,
                                           &exit_code, &reaped);
        serial_print(status == AGENT_OS_PROCESS_OK
                         ? "SYSCALL wait OK - child reaped\r\n"
                         : "SYSCALL wait EBUSY - no zombie\r\n");
        frame->rax = status == AGENT_OS_PROCESS_OK
            ? (uint64_t)exit_code : (uint64_t)-16;
        return;
    }
    if (frame->rax == SYS_IPC_CANCEL) {
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        AgentOsProcess *target;
        if (agent_os_process_lookup(&process_table,
                                    (AgentOsProcessId)frame->rdi,
                                    &target) != AGENT_OS_PROCESS_OK ||
            target->parent != current_id) {
            serial_print("SYSCALL ipc cancel denied\r\n");
            frame->rax = (uint64_t)-1;
            return;
        }
        int status = agent_os_process_cancel_ipc(
            &process_table, (AgentOsProcessId)frame->rdi);
        serial_print(status == AGENT_OS_PROCESS_OK
                         ? "SYSCALL ipc cancel OK\r\n"
                         : "SYSCALL ipc cancel EBUSY\r\n");
        frame->rax = status == AGENT_OS_PROCESS_OK ? 0 : (uint64_t)-16;
        return;
    }
    if (frame->rax == SYS_KILL) {
        /* G2 fixture gate: production kill must additionally present a
         * capability checked by the G3 authority before this branch is
         * exposed to general user processes. */
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        AgentOsProcess *target;
        if (agent_os_process_lookup(&process_table,
                                    (AgentOsProcessId)frame->rdi,
                                    &target) != AGENT_OS_PROCESS_OK ||
            target->parent != current_id) {
            frame->rax = (uint64_t)-1;
            return;
        }
        int status = agent_os_process_kill(&process_table,
                                           (AgentOsProcessId)frame->rdi,
                                           (int64_t)frame->rsi);
        serial_print(status == AGENT_OS_PROCESS_OK
                         ? "SYSCALL kill OK - child zombie\r\n"
                         : "SYSCALL kill denied\r\n");
        frame->rax = status == AGENT_OS_PROCESS_OK ? 0 : (uint64_t)-3;
        return;
    }
    if (frame->rax == SYS_GROUP_TERMINATE) {
        /* Group lifecycle is parent-scoped in Ring 0: a supervisor can only
         * terminate its direct children, and a group id is never a global
         * authority or a way to reach the caller itself. */
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        int count = agent_os_process_group_terminate(
            &process_table, current_id, frame->rdi, (int64_t)frame->rsi);
        serial_print(count >= 0 ? "SYSCALL group terminate OK\r\n"
                                : "SYSCALL group terminate denied\r\n");
        frame->rax = count >= 0 ? (uint64_t)count : (uint64_t)-1;
        return;
    }
    if (frame->rax == SYS_GROUP_TERMINATE_TREE) {
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        int count = agent_os_process_group_terminate_tree(
            &process_table, current_id, frame->rdi, (int64_t)frame->rsi);
        serial_print(count >= 0 ? "SYSCALL group terminate tree OK\r\n"
                                : "SYSCALL group terminate tree denied\r\n");
        frame->rax = count >= 0 ? (uint64_t)count : (uint64_t)-1;
        return;
    }
    if (frame->rax == SYS_GROUP_FREEZE || frame->rax == SYS_GROUP_RESUME) {
        if (current_id == AGENT_OS_PROCESS_INVALID) { frame->rax = (uint64_t)-3; return; }
        int count = frame->rax == SYS_GROUP_FREEZE
            ? agent_os_process_group_freeze(&process_table, current_id, frame->rdi)
            : agent_os_process_group_resume(&process_table, current_id, frame->rdi);
        serial_print(frame->rax == SYS_GROUP_FREEZE
                         ? "SYSCALL group freeze OK\r\n"
                         : "SYSCALL group resume OK\r\n");
        frame->rax = count >= 0 ? (uint64_t)count : (uint64_t)-1;
        return;
    }
    if (frame->rax == SYS_RESTART) {
        /* G4 fixture authority: only a live parent may restart its own
         * zombie child.  This is deliberately narrower than process create;
         * capability-backed service spawning will replace it later. */
        if (current_id == AGENT_OS_PROCESS_INVALID) {
            frame->rax = (uint64_t)-3;
            return;
        }
        AgentOsProcess *target;
        if (agent_os_process_lookup(&process_table,
                                    (AgentOsProcessId)frame->rdi,
                                    &target) != AGENT_OS_PROCESS_OK ||
            target->parent != current_id ||
            target->state != AGENT_OS_PROCESS_ZOMBIE) {
            serial_print("SYSCALL restart denied\r\n");
            frame->rax = (uint64_t)-1;
            return;
        }
        int status = agent_os_process_restart(
            &process_table, (AgentOsProcessId)frame->rdi);
        serial_print(status == AGENT_OS_PROCESS_OK
                         ? "SYSCALL restart OK - service READY\r\n"
                         : "SYSCALL restart failed\r\n");
        frame->rax = status == AGENT_OS_PROCESS_OK ? 0 : (uint64_t)-16;
        return;
    }
    if (frame->rax == SYS_EXIT) {
        if (current_id == AGENT_OS_PROCESS_INVALID ||
            agent_os_process_exit(&process_table, current_id,
                                  (int64_t)frame->rdi) != AGENT_OS_PROCESS_OK) {
            scheduler_halt("SYSCALL exit failed");
        }
        serial_print("SYSCALL exit OK - user reclaimed\r\n");
        if (!schedule_into_next(frame)) {
            scheduler_halt("SCHEDULER idle - all tasks exited");
        }
        serial_print("SCHEDULER switched task\r\n");
        return;
    }
    serial_print("SYSCALL denied - unknown number\r\n");
    frame->rax = (uint64_t)-38;
}

void kernel_exception_handler(ExceptionFrame *frame) {
#if defined(AGENT_OS_TEST_TIMER_PREEMPT)
    if (frame != 0 && frame->vector == 32 && (frame->cs & 3u) == 3u) {
        outb(0x20, 0x20);
        (void)preempt_user_timer(frame);
        return;
    }
#endif
    serial_print("EXCEPTION observed\r\n");
    serial_exception_hex("vector=", frame->vector);
    serial_exception_hex("error=", frame->error_code);
    serial_exception_hex("rip=", frame->rip);
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    serial_exception_hex("cr2=", cr2);
#if defined(AGENT_OS_TEST_FAULT_RECOVERY) || \
    defined(AGENT_OS_TEST_SUPERVISOR_FAULT)
    if (frame->vector == 14 && (frame->cs & 3u) == 3u) {
        if (recover_user_exception(frame)) {
#if defined(AGENT_OS_TEST_SUPERVISOR_FAULT)
            serial_print("G4 Supervisor service #PF -> zombie\r\n");
#else
            serial_print("G2 user fault -> task zombie\r\n");
            serial_print("G2 fault recovery scheduled next task\r\n");
#endif
            return;
        }
        scheduler_halt("G2 fault recovery failed");
    }
#endif
    serial_print("EXCEPTION HALT\r\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void kernel_main(const BootInfo *boot_info) {
    vm_init();
    serial_print("VM G1 4K NX map OK\r\n");
    PhysAllocator allocator;
    int pmm_ready = phys_allocator_init(&allocator, boot_info);
    vm_set_phys_allocator(pmm_ready ? &allocator : 0);
    pic_mask_all();
    interrupts_init();
#if defined(AGENT_OS_TEST_TIMER_PREEMPT)
    pic_timer_init();
#endif
    serial_print("IDT OK - 256 vectors\r\n");
    serial_print("KERNEL C OK\r\n");
    AgentOsVirtioProbe virtio_probe;
    if (agent_os_virtio_probe_block(&virtio_probe)) {
        serial_print("G6 virtio block transport READY\r\n");
        if (agent_os_virtio_block_read_sector0(&virtio_probe)) {
            serial_print("G6 virtio block READ OK\r\n");
        } else {
            serial_print("G6 virtio block READ FAIL\r\n");
        }
    }
    if (agent_os_virtio_probe_net(&virtio_probe)) {
        serial_print("G6 virtio net transport READY\r\n");
#if defined(AGENT_OS_TEST_G6_VIRTIO_NET)
        if (agent_os_virtio_net_send_test_packet(&virtio_probe)) {
            serial_print("G6 virtio net TX OK\r\n");
        } else {
            serial_print("G6 virtio net TX FAIL\r\n");
        }
#endif
#if defined(AGENT_OS_TEST_G6_VIRTIO_NET_RX)
        int rx_status = agent_os_virtio_net_receive_test_packet(&virtio_probe);
        if (rx_status >= 0) {
            serial_print("G6 virtio net RX ARMED\r\n");
            serial_print(rx_status == 1 ? "G6 virtio net RX OK\r\n"
                                        : "G6 virtio net RX WAIT\r\n");
        } else {
            serial_print("G6 virtio net RX BOUNDARY FAIL\r\n");
        }
#endif
    }
    if (agent_os_virtio_probe_input(&virtio_probe)) {
        serial_print(virtio_probe.queue_ready
                         ? "G6 virtio input transport READY\r\n"
                         : (virtio_probe.modern_caps
                                ? "G6 virtio input modern CAPABILITIES DISCOVERED\r\n"
                                : "G6 virtio input transport DISCOVERED\r\n"));
#if defined(AGENT_OS_TEST_G6_VIRTIO_INPUT)
        if (virtio_probe.queue_ready) {
            AgentOsVirtioInputEvent input_event;
            int input_status = agent_os_virtio_input_read_event(&virtio_probe,
                                                                 &input_event);
            if (input_status >= 0) {
                serial_print("G6 virtio input queue ARMED\r\n");
                if (input_status == 1) {
                    serial_print("G6 virtio input EVENT OK type=");
                    serial_hex(input_event.type);
                    serial_print(" code=");
                    serial_hex(input_event.code);
                    serial_print(" value=");
                    serial_hex(input_event.value);
                    serial_print("\r\n");
                } else {
                    serial_print("G6 virtio input EVENT WAIT\r\n");
                }
            } else {
                serial_print("G6 virtio input queue BOUNDARY FAIL\r\n");
            }
        }
#endif
    }
    serial_print("boot_info.magic=");
    serial_hex(boot_info->magic);
    serial_print("\r\n");
    serial_print("boot_info.version=");
    serial_hex(boot_info->version);
    serial_print("\r\n");
    serial_print("boot_info.loader_type=");
    serial_hex(boot_info->loader_type);
    serial_print("\r\n");
    serial_print("e820=[");
    serial_hex(boot_info->memory_map_addr);
    serial_print(", count=");
    serial_hex(boot_info->memory_map_count);
    serial_print(", usable=");
    serial_hex(count_usable_e820(boot_info));
    serial_print("]\r\n");
    serial_print("vm.identity_end=");
    serial_hex(AGENT_OS_IDENTITY_MAP_END);
    serial_print(" high_half_target=");
    serial_hex(AGENT_OS_HIGH_HALF_KERNEL_BASE);
    serial_print(" flags=");
    serial_hex(boot_info->flags);
    serial_print("\r\n");
    serial_print("kernel=[");
    serial_hex(boot_info->kernel_phys_start);
    serial_print(", ");
    serial_hex(boot_info->kernel_phys_end);
    serial_print("]\r\n");
    serial_print("pmm.ready=");
    serial_hex((uint64_t)pmm_ready);
    serial_print(" first_page=");
    serial_hex(allocator.next_page);
    serial_print("\r\n");

#if defined(AGENT_OS_TEST_DIV0)
    {
        register uint64_t divisor __asm__("rcx") = 0;
        register uint64_t dividend __asm__("rax") = 1;
        __asm__ volatile ("xor %%rdx, %%rdx; divq %%rcx"
                          : "+a"(dividend)
                          : "c"(divisor)
                          : "rdx", "cc");
    }
#endif

#if defined(AGENT_OS_TEST_RING3)
    tss_init();
    agent_os_process_table_init(&process_table);
    agent_os_ipc_endpoint_init(&ipc_endpoint);
    for (uint32_t endpoint_index = 1;
         endpoint_index < AGENT_OS_IPC_ENDPOINT_POOL_SIZE; ++endpoint_index) {
        ipc_endpoints[endpoint_index] = (AgentOsIpcEndpoint){0};
    }
    agent_os_policy_token_table_init(&policy_tokens);
    policy_token_clock = 0;
    ipc_wait_sequence = 0;
    policy_emergency_paused = 0;
    ipc_ready = 1;
    const uint8_t *service_image = user_image_start;
    uint64_t service_image_size =
        (uint64_t)(uintptr_t)user_image_end -
        (uint64_t)(uintptr_t)user_image_start;
    if ((boot_info->flags & AGENT_OS_BOOT_V2_FLAG_INITRD) != 0) {
        serial_print("G4 initrd=[");
        serial_hex(boot_info->initrd_phys_start);
        serial_print(", ");
        serial_hex(boot_info->initrd_phys_end);
        serial_print("]\r\n");
        if (!initrd_service_image(boot_info, &service_image,
                                  &service_image_size)) {
            scheduler_halt("G4 initrd validation failed");
        }
        serial_print("G4 initrd manifest OK\r\n");
        serial_print("G4 Supervisor service image READY\r\n");
    }
    AgentOsElfLoadPlan user_plan;
    AgentOsStatus user_load_status = agent_os_elf64_load(
        service_image, service_image_size,
        user_load_storage, UINT64_C(0x00400000), sizeof(user_load_storage),
        &user_plan);
    if (user_load_status != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER user ELF load failed");
    }
    AgentOsStatus space_status = vm_process_address_space_init(&process_spaces[0]);
    if (space_status != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER address space setup failed");
    }
    space_status = vm_process_address_space_init(&process_spaces[1]);
    if (space_status != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER address space setup failed");
    }
    if (vm_map_phys_allocator_pages(&process_spaces[0]) != AGENT_OS_OK ||
        vm_map_phys_allocator_pages(&process_spaces[1]) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER PMM page-table aliases failed");
    }
#if defined(AGENT_OS_TEST_IPC_BLOCKING_MULTI) || \
    defined(AGENT_OS_TEST_GROUP_RECURSIVE)
    space_status = vm_process_address_space_init(&process_spaces[2]);
    if (space_status != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER address space setup failed");
    }
#endif
    /* Migration preparation gate: both independent roots carry the same
     * kernel-owned high-half aliases, while execution still remains on the
     * low identity mapping until a later relocation step is proven. */
    uint64_t high_probe = vm_high_half_address(
        (uint64_t)(uintptr_t)__kernel_text_start);
    uint64_t high_physical;
    uint64_t high_flags;
    if (high_probe == 0 ||
        agent_os_address_space_lookup(&process_spaces[0], high_probe,
                                      &high_physical, &high_flags) != AGENT_OS_OK ||
        high_physical != (uint64_t)(uintptr_t)__kernel_text_start ||
        (high_flags & (AGENT_OS_VM_READ | AGENT_OS_VM_EXEC)) !=
            (AGENT_OS_VM_READ | AGENT_OS_VM_EXEC) ||
        agent_os_address_space_lookup(&process_spaces[1], high_probe,
                                      &high_physical, &high_flags) != AGENT_OS_OK ||
        high_physical != (uint64_t)(uintptr_t)__kernel_text_start) {
        scheduler_halt("HIGH HALF alias contract failed");
    }
    serial_print("HIGH HALF alias contract READY low-exec preserved\r\n");
    /* The bootstrap root does not contain the high-half window. Switch to a
     * completed process root before the probe; its low kernel mappings keep
     * the current RIP/data/stack reachable during this bounded transition. */
    vm_load_address_space(process_spaces[0].root_physical);
    uint64_t probe_low = (uint64_t)(uintptr_t)&high_half_rip_probe;
    uint64_t probe_high = vm_high_half_address(probe_low);
    if (probe_high == 0 || probe_high == probe_low) {
        scheduler_halt("HIGH HALF RIP probe address failed");
    }
    uint64_t (*high_probe_fn)(void) =
        (uint64_t (*)(void))(uintptr_t)probe_high;
    if (high_probe_fn() != UINT64_C(0x48495250)) {
        scheduler_halt("HIGH HALF RIP probe failed");
    }
    serial_print("HIGH HALF RIP probe OK stack low preserved\r\n");
    vm_restore_bootstrap_address_space();
    for (uint32_t index = 0; index < user_plan.segment_count; ++index) {
        const AgentOsElfLoadSegment *segment = &user_plan.segments[index];
        uint64_t length = (segment->memory_size + 0xFFF) & ~UINT64_C(0xFFF);
        uint64_t flags = 0;
        if (segment->flags & AGENT_OS_ELF_R) flags |= AGENT_OS_VM_READ;
        if (segment->flags & AGENT_OS_ELF_W) flags |= AGENT_OS_VM_WRITE;
        if (segment->flags & AGENT_OS_ELF_X) flags |= AGENT_OS_VM_EXEC;
        uint64_t storage_offset = segment->virtual_address - UINT64_C(0x00400000);
        if (storage_offset + length > sizeof(user_load_storage) ||
            agent_os_address_space_map(
                &process_spaces[0], segment->virtual_address,
                (uint64_t)(uintptr_t)user_load_storage + storage_offset,
                length, flags | AGENT_OS_VM_USER) != AGENT_OS_OK) {
            scheduler_halt("SCHEDULER user ELF map failed");
        }
    }
    AgentOsProcessSpec first = {
        .entry_rip = user_plan.entry,
        .user_rsp = UINT64_C(0x001FF000),
        .address_space_root = process_spaces[0].root_physical,
        .kernel_stack_top = (uint64_t)(uintptr_t)kernel_stack_top,
        .parent = AGENT_OS_PROCESS_INVALID,
        .group_id = 1,
        .flags = 0,
    };
    AgentOsProcessSpec second = first;
    second.entry_rip = (uint64_t)(uintptr_t)user_entry_secondary;
    second.user_rsp = UINT64_C(0x001FD000);
    second.address_space_root = process_spaces[1].root_physical;
    second.group_id = 7;
#if defined(AGENT_OS_TEST_GROUP_RECURSIVE)
    second.group_id = 9;
#endif
    if (first.address_space_root == second.address_space_root) {
        scheduler_halt("SCHEDULER address space roots aliased");
    }
    if (agent_os_process_create(&process_table, &first, &bootstrap_process) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER bootstrap failed");
    }
    AgentOsProcess *bootstrap_record;
    if (agent_os_process_lookup(&process_table, bootstrap_process,
                                &bootstrap_record) != AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER bootstrap lookup failed");
    }
    agent_os_capability_table_init(&bootstrap_record->capabilities);
    CapabilityHandle bootstrap_ipc_capability;
    if (agent_os_capability_mint(
            &bootstrap_record->capabilities,
            (uint64_t)(uintptr_t)&ipc_endpoint,
            CAP_RIGHT_SEND | CAP_RIGHT_RECV | CAP_RIGHT_TRANSFER |
                CAP_RIGHT_REVOKE,
            &bootstrap_ipc_capability) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER capability setup failed");
    }
    CapabilityHandle bootstrap_shm_capability;
    if (agent_os_capability_mint(
            &bootstrap_record->capabilities,
            (uint64_t)(uintptr_t)shared_test_page,
            CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_MAP,
            &bootstrap_shm_capability) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER shared memory setup failed");
    }
    second.parent = bootstrap_process;
    if (agent_os_process_create(&process_table, &second, &secondary_process) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER bootstrap failed");
    }
    AgentOsProcess *secondary_record;
    if (agent_os_process_lookup(&process_table, secondary_process,
                                &secondary_record) != AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER secondary lookup failed");
    }
    agent_os_capability_table_init(&secondary_record->capabilities);
    CapabilityHandle secondary_ipc_capability;
    CapabilityHandle secondary_shm_capability;
    if (agent_os_capability_mint(
            &secondary_record->capabilities,
            (uint64_t)(uintptr_t)&ipc_endpoint,
            CAP_RIGHT_SEND | CAP_RIGHT_RECV,
            &secondary_ipc_capability) != AGENT_OS_OK ||
        agent_os_capability_mint(
            &secondary_record->capabilities,
            (uint64_t)(uintptr_t)shared_test_page,
            CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_MAP,
            &secondary_shm_capability) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER secondary capability setup failed");
    }
    CapabilityHandle secondary_policy_capability;
    if (agent_os_capability_mint(
            &secondary_record->capabilities,
            (uint64_t)(uintptr_t)&policy_authority,
            CAP_RIGHT_ADMIN,
            &secondary_policy_capability) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER policy capability setup failed");
    }
#if defined(AGENT_OS_TEST_IPC_BLOCKING_MULTI)
    AgentOsProcessSpec third = first;
    third.entry_rip = (uint64_t)(uintptr_t)user_entry_tertiary;
    third.user_rsp = UINT64_C(0x001FB000);
    third.address_space_root = process_spaces[2].root_physical;
    third.parent = bootstrap_process;
    if (agent_os_process_create(&process_table, &third, &tertiary_process) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER tertiary bootstrap failed");
    }
    AgentOsProcess *tertiary_record;
    if (agent_os_process_lookup(&process_table, tertiary_process,
                                &tertiary_record) != AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER tertiary lookup failed");
    }
    agent_os_capability_table_init(&tertiary_record->capabilities);
    if (agent_os_capability_mint(
            &tertiary_record->capabilities,
            (uint64_t)(uintptr_t)&ipc_endpoint,
            CAP_RIGHT_SEND | CAP_RIGHT_RECV,
            &secondary_ipc_capability) != AGENT_OS_OK) {
        scheduler_halt("SCHEDULER tertiary capability setup failed");
    }
#elif defined(AGENT_OS_TEST_GROUP_RECURSIVE)
    AgentOsProcessSpec third = first;
    third.entry_rip = (uint64_t)(uintptr_t)user_entry_tertiary;
    third.user_rsp = UINT64_C(0x001FB000);
    third.address_space_root = process_spaces[2].root_physical;
    third.parent = secondary_process;
    third.group_id = 9;
    if (agent_os_process_create(&process_table, &third, &tertiary_process) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER recursive tertiary bootstrap failed");
    }
    AgentOsProcess *tertiary_record;
    if (agent_os_process_lookup(&process_table, tertiary_process,
                                &tertiary_record) != AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER recursive tertiary lookup failed");
    }
    agent_os_capability_table_init(&tertiary_record->capabilities);
#endif
    AgentOsProcessId first_run;
    if (agent_os_process_schedule(&process_table, &first_run) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER no runnable task");
    }
    AgentOsProcess *initial;
    if (agent_os_process_lookup(&process_table, first_run, &initial) !=
        AGENT_OS_PROCESS_OK) {
        scheduler_halt("SCHEDULER lookup failed");
    }
    serial_print("G2 address spaces READY distinct CR3 roots\r\n");
    serial_print(vm_page_tables_use_phys_allocator()
                     ? "G2 page tables PMM-backed identity-safe\r\n"
                     : "G2 page tables fallback bounded pool\r\n");
#if defined(AGENT_OS_TEST_IPC_BLOCKING_MULTI) || \
    defined(AGENT_OS_TEST_GROUP_RECURSIVE)
    serial_print("G2 scheduler READY tasks=3\r\n");
#else
    serial_print("G2 scheduler READY tasks=2\r\n");
#endif
    serial_print("RING3 launch\r\n");
    uint64_t elf_physical;
    uint64_t elf_flags;
    AgentOsStatus elf_lookup = agent_os_address_space_lookup(
        &process_spaces[0], user_plan.entry, &elf_physical, &elf_flags);
    if (elf_lookup != AGENT_OS_OK ||
        (elf_flags & (AGENT_OS_VM_USER | AGENT_OS_VM_EXEC)) !=
            (AGENT_OS_VM_USER | AGENT_OS_VM_EXEC)) {
        scheduler_halt("SCHEDULER user ELF permission check failed");
    }
    serial_print("G2 ELF user load OK\r\n");
    if ((boot_info->flags & AGENT_OS_BOOT_V2_FLAG_INITRD) != 0) {
        serial_print("G4 Supervisor spawned service from initrd\r\n");
    }
    vm_load_address_space(initial->address_space_root);
    serial_print("G2 CR3 switch OK\r\n");
    enter_user_mode(initial->frame.rip, initial->frame.rsp);
#else
    serial_print("RING3 ABI READY - launch gated pending TSS\r\n");
#endif

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

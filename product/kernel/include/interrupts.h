#ifndef AGENT_OS_INTERRUPTS_H
#define AGENT_OS_INTERRUPTS_H

#include <stdint.h>

typedef struct __attribute__((packed)) IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} IdtEntry;

typedef struct __attribute__((packed)) Idtr {
    uint16_t limit;
    uint64_t base;
} Idtr;

typedef struct ExceptionFrame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} ExceptionFrame;

typedef struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} SyscallFrame;

typedef struct __attribute__((packed)) Tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint32_t reserved2;
    uint32_t reserved3;
    uint16_t iomap_base;
    uint16_t reserved4;
} Tss64;

_Static_assert(sizeof(Tss64) == 104, "TSS ABI size changed");

void interrupts_init(void);
/* The exception path may replace the interrupted frame with a runnable
 * process frame when a Ring 3 task faults.  The assembly epilogue then
 * restores the replacement and iretq resumes that task. */
void kernel_exception_handler(ExceptionFrame *frame);
void kernel_syscall_handler(SyscallFrame *frame);

#endif

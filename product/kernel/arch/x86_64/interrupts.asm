; Minimal x86_64 exception entry stubs for M5.

bits 64

default rel

global isr_stub_table
global isr_common
global isr_syscall
extern kernel_exception_handler
extern kernel_syscall_handler

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro

; CPU-pushed error-code exceptions: #DF, #TS, #NP, #SS, #GP, #PF, #AC.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; All non-exception vectors use one diagnostic stop path for now.
isr_stub_spurious:
    push qword 0
    push qword 255
    jmp isr_common

; IRQ0 after the PIC is remapped by the timer test path.
isr_stub_timer:
    push qword 0
    push qword 32
    jmp isr_common

; Entry stack seen by C (top first after register saves):
; r15..rax, vector, error, RIP, CS, RFLAGS, [RSP, SS].
isr_common:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call kernel_exception_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16                    ; discard vector and error code
    iretq

; DPL3 int 0x80 entry. Preserve the general registers, let C inspect the
; syscall frame, then restore and return with iretq.  The C handler may replace
; the frame with another cooperative task before this epilogue runs.
isr_syscall:
    cld
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call kernel_syscall_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq

align 8
isr_stub_table:
%assign vector 0
%rep 32
    dq isr_stub_%+vector
%assign vector vector + 1
%endrep
 dq isr_stub_timer
%rep 223
    dq isr_stub_spurious
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits

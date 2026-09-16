; 64-bit C entry and a private bootstrap stack.

bits 64

global kernel_entry
global enter_user_mode
global kernel_stack_top
global high_half_rip_probe
global load_kernel_gdt
extern kernel_main
extern user_entry

section .text.entry
kernel_entry:
    cli
    lea rsp, [rel kernel_stack_top]
    and rsp, -16
    call kernel_main
.halt:
    hlt
    jmp .halt

; Install a descriptor table owned by the kernel.  The caller keeps the
; existing code selector (0x18), whose descriptor has the same meaning in
; the kernel table, so no far control transfer is needed here.
load_kernel_gdt:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    ; UEFI enters with firmware-owned CS, while SeaBIOS happens to use the
    ; same selector value as the kernel table.  Reload CS explicitly so both
    ; launch paths execute under the kernel-owned descriptor table.
    push qword 0x18
    lea rax, [rel .kernel_cs_loaded]
    push rax
    retfq
.kernel_cs_loaded:
    ret

; Transfer once to a scheduled Ring 3 process.  The C scheduler loads the
; process CR3 before entering this trampoline and before every frame switch.
enter_user_mode:
    cli
    ; RDX carries the initial opaque argument register selected by the
    ; bootstrap process.  Normal fixtures pass zero; test-gated native
    ; services use it for a capability handle.
    mov rbx, rdx
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    push qword 0x23
    push rsi
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push qword 0x2B
    push rdi
    iretq

; Position-independent probe used only after a process root has a kernel-owned
; high-half alias. It deliberately leaves RSP unchanged so this slice proves
; high-half RIP execution without claiming stack relocation.
high_half_rip_probe:
    mov eax, 0x48495250       ; "HIRP"
    ret

section .bss
align 16
kernel_stack:
    resb 16384
kernel_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits

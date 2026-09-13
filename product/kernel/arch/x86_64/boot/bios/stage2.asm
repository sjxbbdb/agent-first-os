; Agent-First OS — BIOS Stage 2 / M4 ELF loader
; Loads a bounded ELF64 kernel, enters long mode, builds BootInfo, and jumps
; to the kernel entry point. This is intentionally single-core and identity
; maps only the first 2 MiB while the higher-half transition is pending.

org 0x8000

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 16
%endif
%ifndef KERNEL_LBA
%define KERNEL_LBA 17
%endif
%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 1
%endif
%ifndef KERNEL_BYTES
%define KERNEL_BYTES 512
%endif
%ifndef INITRD_LBA
%define INITRD_LBA (KERNEL_LBA + KERNEL_SECTORS)
%endif
%ifndef INITRD_SECTORS
%define INITRD_SECTORS 1
%endif
%ifndef INITRD_BYTES
%define INITRD_BYTES 512
%endif

%define KERNEL_STAGING 0x10000
%define KERNEL_LOAD_MIN 0x100000
%define KERNEL_LOAD_MAX 0x200000
%define INITRD_STAGING 0x30000
%define BOOTINFO_ADDR 0x6000
%define E820_ADDR 0x5000
%define E820_MAX_ENTRIES 32

bits 16
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000
    cld
    mov [boot_drive16], dl
    call serial_init16

    mov si, message_stage2
    call print_both16
    call load_kernel16
    jc .halt
    call load_initrd16
    jc .halt
    call collect_e820_16

    call check_long_mode16
    jc .halt
    call enable_a20_16
    call enter_protected_mode16

.halt:
    cli
    hlt
    jmp .halt

serial_init16:
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    ret

print_both16:
.next:
    lodsb
    test al, al
    jz .done
    push ax
    call serial_write16
    pop ax
    mov ah, 0x0E
    xor bh, bh
    int 0x10
    jmp .next
.done:
    ret

serial_write16:
    push ax
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, 0x3F8
    out dx, al
    ret

load_kernel16:
    mov word [kernel_remaining], KERNEL_SECTORS
    mov word [kernel_load_segment], KERNEL_STAGING >> 4
    mov dword [kernel_current_lba], KERNEL_LBA
    mov dword [kernel_current_lba + 4], 0
.chunk:
    cmp word [kernel_remaining], 128
    jbe .count_ready
    mov word [kernel_dap_count], 128
    jmp .read
.count_ready:
    mov ax, [kernel_remaining]
    mov [kernel_dap_count], ax
.read:
    mov ax, [kernel_load_segment]
    mov [kernel_dap_segment], ax
    mov eax, [kernel_current_lba]
    mov [kernel_dap_lba], eax
    mov eax, [kernel_current_lba + 4]
    mov [kernel_dap_lba + 4], eax
    mov byte [kernel_retries], 3
.retry:
    mov si, kernel_dap
    mov dl, [boot_drive16]
    mov ah, 0x42
    int 0x13
    jnc .chunk_ok
    mov si, disk_error
    call print_both16
    xor ah, ah
    mov dl, [boot_drive16]
    int 0x13
    dec byte [kernel_retries]
    jnz .retry
    stc
    ret
.chunk_ok:
    xor eax, eax
    mov ax, [kernel_dap_count]
    sub [kernel_remaining], ax
    add [kernel_current_lba], eax
    adc dword [kernel_current_lba + 4], 0
    shl eax, 5
    add [kernel_load_segment], ax
    cmp word [kernel_remaining], 0
    jne .chunk
    clc
    ret

load_initrd16:
    mov byte [initrd_retries], 3
.retry:
    mov si, initrd_dap
    mov dl, [boot_drive16]
    mov ah, 0x42
    int 0x13
    jnc .ok
    mov si, disk_error_initrd
    call print_both16
    xor ah, ah
    mov dl, [boot_drive16]
    int 0x13
    dec byte [initrd_retries]
    jnz .retry
    stc
    ret
.ok:
    clc
    ret

check_long_mode16:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    jz .no_cpuid
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long
    clc
    ret
.no_cpuid:
    mov si, error_no_long
    call print_both16
    stc
    ret
.no_long:
    mov si, error_no_long
    call print_both16
    stc
    ret

enable_a20_16:
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    ret

; Collect up to 32 BIOS E820 entries into a fixed low-memory buffer. A missing
; or malformed map is reported through a zero count; boot can continue because
; the current identity map is deliberately bounded and temporary.
collect_e820_16:
    xor ebx, ebx
    xor bp, bp
    mov di, E820_ADDR
.next:
    cmp bp, E820_MAX_ENTRIES
    jae .done
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .empty
    cmp ecx, 20
    jb .empty
    inc bp
    add di, 24
    test ebx, ebx
    jnz .next
.done:
    mov [e820_count], bp
    ret
.empty:
    xor bp, bp
    mov [e820_count], bp
    ret

enter_protected_mode16:
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_entry

bits 32
protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7000
    mov eax, 0x1000
    mov edi, eax
    xor eax, eax
    mov ecx, 0x4000 / 4
    rep stosd

    ; Build a 4 KiB identity map for the bootstrap window.  Every page is
    ; supervisor-only except the statically linked user image at 0x180000.
    ; Keeping the user bit at the leaf entries gives Ring 3 a real page-table
    ; boundary while the process/address-space subsystem is still pending.
    mov dword [0x1000], 0x2000 | 0x7
    mov dword [0x2000], 0x3000 | 0x7
    mov dword [0x3000], 0x4000 | 0x7
    mov edi, 0x4000
    mov eax, 0x3
    mov ecx, 512
.fill_pt:
    mov [edi], eax
    add edi, 8
    add eax, 0x1000
    loop .fill_pt

    ; PTEs 384..511 cover 0x180000..0x1fffff (user text and stack).
    mov edi, 0x4000 + (0x180000 / 0x1000) * 8
    mov ecx, 128
.mark_user_pt:
    or dword [edi], 0x4
    add edi, 8
    loop .mark_user_pt
    mov eax, 0x1000
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)
    mov cr0, eax
    jmp 0x18:long_mode_entry

bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x7000
    mov rsi, message_long_mode
    call serial_write_string64
    call load_elf64
    jc .halt
    mov rdi, BOOTINFO_ADDR
    mov rsi, message_kernel_ok
    call serial_write_string64
    jmp r9
.halt:
    cli
    hlt
    jmp .halt

; Validate and load PT_LOAD segments from the staged ELF image.
load_elf64:
    mov rbx, KERNEL_STAGING
    cmp dword [rbx], 0x464c457f
    jne .bad_elf
    cmp byte [rbx + 4], 2
    jne .bad_elf
    cmp byte [rbx + 5], 1
    jne .bad_elf
    cmp word [rbx + 18], 0x3e
    jne .bad_elf
    cmp word [rbx + 52], 64
    jb .bad_elf
    cmp word [rbx + 54], 56
    jb .bad_elf

    movzx ecx, word [rbx + 56]
    test ecx, ecx
    jz .bad_elf
    movzx eax, word [rbx + 54]
    mul rcx
    test rdx, rdx
    jnz .bad_elf
    add rax, [rbx + 32]
    jc .bad_elf
    cmp rax, KERNEL_BYTES
    ja .bad_elf

    xor r8, r8
    mov r10, KERNEL_LOAD_MAX
    xor r11, r11
    xor r13, r13
    mov rax, [rbx + 32]
    mov r12, rbx
    add r12, rax
    movzx edx, word [rbx + 54]
    ; Keep the program-header index out of RCX: REP MOVSB/STOSB below
    ; consume RCX as their byte counter.  Using CX here made a second
    ; PT_LOAD segment jump to an arbitrary header after the first copy.
    xor r14d, r14d
.ph_loop:
    cmp r14w, word [rbx + 56]
    jae .ph_done
    cmp dword [r12], 1
    jne .next_ph
    mov rax, [r12 + 32]
    cmp rax, [r12 + 40]
    ja .bad_segment
    mov rdi, [r12 + 8]
    add rdi, rax
    jc .bad_segment
    cmp rdi, KERNEL_BYTES
    ja .bad_segment
    mov rdi, [r12 + 24]
    cmp rdi, KERNEL_LOAD_MIN
    jb .bad_segment
    mov rsi, rdi
    add rsi, [r12 + 40]
    jc .bad_segment
    cmp rsi, KERNEL_LOAD_MAX
    ja .bad_segment
    cmp rdi, r10
    cmovb r10, rdi
    cmp rsi, r11
    cmova r11, rsi
    test dword [r12 + 4], 1
    jz .copy_segment
    mov rax, rdi
    add rax, [r12 + 32]
    cmp rax, r13
    cmova r13, rax
.copy_segment:
    mov rsi, KERNEL_STAGING
    add rsi, [r12 + 8]
    mov rdi, [r12 + 24]
    mov rcx, [r12 + 32]
    rep movsb
    mov rcx, [r12 + 40]
    sub rcx, [r12 + 32]
    xor eax, eax
    rep stosb
    mov r8, 1
.next_ph:
    inc r14w
    add r12, rdx
    jmp .ph_loop
.ph_done:
    test r8, r8
    jz .bad_elf
    test r13, r13
    jz .bad_segment
    mov rax, [rbx + 24]
    cmp rax, r10
    jb .bad_segment
    cmp rax, r13
    jae .bad_segment

    mov rdi, BOOTINFO_ADDR
    xor eax, eax
    mov ecx, 16
    rep stosq
    mov rdi, BOOTINFO_ADDR
    mov rax, 0x41474f53424f4f54
    mov [rdi], rax
    mov word [rdi + 8], 2
    mov word [rdi + 10], 128
    mov dword [rdi + 12], 1
    cmp word [e820_count], 0
    je .no_e820_flag
    or dword [rdi + 12], 2
.no_e820_flag:
    ; Stage2's GDT is bootstrap-only.  The kernel installs and owns its
    ; descriptor table before enabling Ring 3; BootInfo never carries a
    ; pointer into this loader's memory.
    mov dword [rdi + 68], 1 ; AGENT_OS_BOOT_LOADER_BIOS
    mov qword [rdi + 16], E820_ADDR
    movzx eax, word [e820_count]
    mov [rdi + 24], eax
    mov dword [rdi + 28], 24
    movzx eax, byte [boot_drive16]
    mov [rdi + 72], rax
    mov [rdi + 80], r10
    mov [rdi + 88], r11
    mov qword [rdi + 96], KERNEL_STAGING
    mov qword [rdi + 104], KERNEL_STAGING + KERNEL_BYTES
    mov qword [rdi + 112], INITRD_STAGING
    mov qword [rdi + 120], INITRD_STAGING + INITRD_BYTES
    or dword [rdi + 12], 0x20 ; AGENT_OS_BOOT_V2_FLAG_INITRD
    mov r9, [rbx + 24]
    clc
    ret
.bad_elf:
    mov rsi, error_bad_elf
    call serial_write_string64
    stc
    ret
.bad_segment:
    mov rsi, error_bad_segment
    call serial_write_string64
    stc
    ret

serial_write_string64:
.next:
    lodsb
    test al, al
    jz .done
    call serial_write64
    jmp .next
.done:
    ret

serial_write64:
    push rax
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    pop rax
    mov dx, 0x3F8
    out dx, al
    ret

align 8
global gdt_start
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AF9A000000FFFF
    dq 0x00CFF2000000FFFF ; user data, DPL3
    dq 0x00AFFA000000FFFF ; user code, DPL3
    dq 0 ; TSS descriptor, filled by the kernel before Ring 3 launch
    dq 0
    dq 0x00AFFA000000FFFF ; user 64-bit code, DPL3 (selector 0x20)
    dq 0x00CFF2000000FFFF ; user data, DPL3 (selector 0x28)
gdt_descriptor:
    dw gdt_descriptor - gdt_start - 1
    dd gdt_start


boot_drive16 db 0
kernel_retries db 0
initrd_retries db 0
e820_count dw 0
message_stage2 db 'S2 BIOS OK - stage2 loaded', 13, 10, 0
message_long_mode db 'S2 64BIT OK - long mode', 13, 10, 0
error_no_long db 'S2 ERROR - long mode unavailable', 13, 10, 0
disk_error db 'S2 BIOS kernel read failed', 13, 10, 0
disk_error_initrd db 'S2 BIOS initrd read failed', 13, 10, 0
message_kernel_ok db 'KERNEL ELF OK - jumping to kernel', 13, 10, 0
error_bad_elf db 'S2 ERROR - invalid kernel ELF', 13, 10, 0
error_bad_segment db 'S2 ERROR - kernel segment', 13, 10, 0

align 16
kernel_dap:
    db 0x10, 0
kernel_dap_count:
    dw 0
    dw 0
kernel_dap_segment:
    dw 0
kernel_dap_lba:
    dq 0
align 4
initrd_dap:
    db 0x10, 0
    dw INITRD_SECTORS
    dw 0
    dw INITRD_STAGING >> 4
    dq INITRD_LBA

kernel_remaining dw 0
kernel_load_segment dw 0
kernel_current_lba dq 0

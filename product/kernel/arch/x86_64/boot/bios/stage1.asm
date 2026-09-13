; Agent-First OS — BIOS Stage 1
; M1 contract: initialize a known real-mode state, preserve BIOS DL,
; print a visible/serial marker, and stop without claiming later stages.

bits 16
org 0x7C00

%ifndef STAGE2_LBA
%define STAGE2_LBA 1
%endif

%define STAGE2_LOAD 0x8000
%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 1
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    mov [boot_drive], dl
    call serial_init

    mov si, message
    call print_both

    call load_stage2
    jc .halt

    jmp 0:STAGE2_LOAD

.halt:
    cli
    hlt
    jmp .halt

; COM1, 38400 baud, 8 data bits, no parity, 1 stop bit.
serial_init:
    mov dx, 0x3F9
    xor al, al
    out dx, al                 ; disable UART interrupts

    mov dx, 0x3FB
    mov al, 0x80
    out dx, al                 ; enable divisor latch

    mov dx, 0x3F8
    mov al, 0x03
    out dx, al                 ; divisor low byte
    mov dx, 0x3F9
    xor al, al
    out dx, al                 ; divisor high byte

    mov dx, 0x3FB
    mov al, 0x03
    out dx, al                 ; 8N1, disable divisor latch

    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al                 ; enable FIFO, clear it

    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al                 ; IRQs enabled, RTS/DTR set
    ret

; Print a zero-terminated string to VGA teletype and COM1.
print_both:
.next:
    lodsb
    test al, al
    jz .done

    push ax
    call serial_write
    pop ax

    mov ah, 0x0E
    xor bh, bh
    int 0x10
    jmp .next
.done:
    ret

serial_write:
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

load_stage2:
    mov byte [retries], 3
.retry:
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jnc .ok

    mov si, disk_error
    call print_both
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    dec byte [retries]
    jnz .retry
    stc
    ret
.ok:
    clc
    ret

boot_drive db 0
retries db 0
message db 'S1 BIOS OK - stage2 pending', 13, 10, 0
disk_error db 'S1 BIOS disk read failed', 13, 10, 0

align 16
dap:
    db 0x10, 0
    dw STAGE2_SECTORS
    dw STAGE2_LOAD
    dw 0
    dq STAGE2_LBA

times 510 - ($ - $$) db 0
dw 0xAA55

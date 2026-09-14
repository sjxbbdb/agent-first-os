OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_entry)

PHDRS
{
    text PT_LOAD FLAGS(5); /* R_X */
    data PT_LOAD FLAGS(6); /* R_W */
    user PT_LOAD FLAGS(5); /* user R_X */
}

SECTIONS
{
    . = 0x00100000;

    .text : ALIGN(16) {
        __kernel_text_start = .;
        KEEP(*(.text.entry))
        *(.text*)
        __kernel_text_end = .;
    } :text

    .rodata : ALIGN(0x1000) {
        __kernel_rodata_start = .;
        *(.rodata*)
        __kernel_rodata_end = .;
    } :text

    . = ALIGN(0x1000);
    .data : ALIGN(16) {
        __kernel_data_start = .;
        *(.data*)
        __kernel_data_end = .;
    } :data

    .bss : ALIGN(0x1000) {
        __kernel_bss_start = .;
        *(COMMON)
        *(.bss*)
        __kernel_bss_end = .;
    } :data

    /* Keep the bootstrap user image on separate pages after all kernel data.
     * Device queues and test variants can grow BSS; never overlap it with an
     * executable user mapping. Native ELF tasks use the separate 0x400000 VMA. */
    .user_text MAX(0x00180000, ALIGN(0x1000)) : {
        __user_text_start = .;
        KEEP(*(.user_text))
        __user_text_end = .;
    } :user

    __kernel_end = .;
    ASSERT(__kernel_end <= 0x001FA000, "kernel/user bootstrap image overlaps user stack reservation")
}

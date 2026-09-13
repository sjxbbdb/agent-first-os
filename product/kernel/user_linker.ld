OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(user_entry)

PHDRS
{
    user PT_LOAD FLAGS(5); /* R|X */
}

SECTIONS
{
    . = 0x00400000;
    .text : ALIGN(0x1000) {
        *(.user_text)
    } :user
    /DISCARD/ : { *(.note.GNU-stack) }
}

#include <stdint.h>

typedef void *EFI_HANDLE;
typedef uint64_t EFI_STATUS;
typedef uint16_t CHAR16;
#define EFI_SUCCESS ((EFI_STATUS)0)

typedef struct {
    uint64_t _pad0[8];
    void *ConOut;
} EFI_SYSTEM_TABLE_MIN;

typedef EFI_STATUS (*EFI_TEXT_OUTPUT_STRING)(void *self, CHAR16 *string);
typedef struct {
    void *Reset;
    EFI_TEXT_OUTPUT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_MIN;

static inline uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static void serial_puts(const char *s) {
    while (*s) {
        while ((uint8_t)(io_in8(0x3fd) & 0x20u) == 0u) { }
        io_out8(0x3f8, (uint8_t)*s++);
    }
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE_MIN *system_table) {
    (void)image;
    serial_puts("AGENTOS_UEFI_STAGE0\r\n");
    if (system_table && system_table->ConOut) {
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_MIN *out = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_MIN *)system_table->ConOut;
        static CHAR16 marker[] = {'A','G','E','N','T','O','S',' ','U','E','F','I',' ','S','T','A','G','E','0','\r','\n',0};
        if (out->OutputString) (void)out->OutputString(out, marker);
    }
    for (;;) __asm__ volatile ("hlt");
    return EFI_SUCCESS;
}
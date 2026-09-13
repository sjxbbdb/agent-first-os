#include <stdint.h>

#include "boot_info_v2.h"
#include "initrd.h"

typedef uint64_t EFI_STATUS;
typedef uint64_t EFI_HANDLE;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;
typedef uint64_t EFI_TPL;
typedef uint64_t EFI_LBA;
typedef uint64_t EFI_UINTN;
typedef uint16_t CHAR16;

#define EFI_SUCCESS UINT64_C(0)
#define EFI_BUFFER_TOO_SMALL UINT64_C(0x8000000000000005)
#define EFI_LOAD_ERROR UINT64_C(0x8000000000000001)
#define EFI_INVALID_PARAMETER UINT64_C(0x8000000000000002)
#define EFI_NOT_FOUND UINT64_C(0x8000000000000014)

#define EFI_ALLOCATE_ADDRESS 2
#define EFI_LOADER_DATA 4
#define EFI_OPEN_READ UINT64_C(1)
#define EFI_FILE_MODE_READ UINT64_C(1)
#define EFI_FILE_READ_ONLY UINT64_C(1)

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EM_X86_64 62
#define ET_EXEC 2

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} EfiGuid;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EfiTableHeader;

typedef EFI_STATUS (*EfiRaiseTpl)(EFI_TPL);
typedef void (*EfiRestoreTpl)(EFI_TPL);
typedef EFI_STATUS (*EfiAllocatePages)(uint32_t, uint32_t, EFI_UINTN,
                                       EFI_PHYSICAL_ADDRESS *);
typedef EFI_STATUS (*EfiFreePages)(EFI_PHYSICAL_ADDRESS, EFI_UINTN);
typedef EFI_STATUS (*EfiGetMemoryMap)(EFI_UINTN *, void *, EFI_UINTN *,
                                      EFI_UINTN *, uint32_t *);
typedef EFI_STATUS (*EfiAllocatePool)(uint32_t, EFI_UINTN, void **);
typedef EFI_STATUS (*EfiFreePool)(void *);
typedef EFI_STATUS (*EfiHandleProtocol)(EFI_HANDLE, EfiGuid *, void **);
typedef EFI_STATUS (*EfiExitBootServices)(EFI_HANDLE, EFI_UINTN);
typedef EFI_STATUS (*EfiLocateProtocol)(EfiGuid *, void *, void **);

typedef struct {
    EfiTableHeader header;
    EfiRaiseTpl raise_tpl;
    EfiRestoreTpl restore_tpl;
    EfiAllocatePages allocate_pages;
    EfiFreePages free_pages;
    EfiGetMemoryMap get_memory_map;
    EfiAllocatePool allocate_pool;
    EfiFreePool free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    EfiHandleProtocol handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    EfiExitBootServices exit_boot_services;
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    EfiLocateProtocol locate_protocol;
} EfiBootServices;

typedef EFI_STATUS (*EfiOutputString)(void *, CHAR16 *);
typedef struct {
    void *reset;
    EfiOutputString output_string;
} EfiTextOutput;

typedef struct {
    EfiTableHeader header;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t firmware_revision_pad;
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    EfiTextOutput *con_out;
    EFI_HANDLE standard_error_handle;
    void *std_err;
    void *runtime_services;
    EfiBootServices *boot_services;
    uint32_t number_of_table_entries;
    void *configuration_table;
} EfiSystemTable;

typedef struct {
    EfiGuid vendor_guid;
    void *vendor_table;
} EfiConfigurationTable;

typedef struct {
    uint32_t revision;
    uint32_t revision_pad;
    EFI_HANDLE parent_handle;
    EfiSystemTable *system_table;
    EFI_HANDLE device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
} EfiLoadedImage;

typedef struct EfiFile EfiFile;
typedef EFI_STATUS (*EfiFileOpen)(EfiFile *, EfiFile **, CHAR16 *, uint64_t,
                                  uint64_t);
typedef EFI_STATUS (*EfiFileClose)(EfiFile *);
typedef EFI_STATUS (*EfiFileRead)(EfiFile *, EFI_UINTN *, void *);
typedef EFI_STATUS (*EfiFileSetPosition)(EfiFile *, uint64_t);
struct EfiFile {
    uint64_t revision;
    EfiFileOpen open;
    EfiFileClose close;
    void *delete_file;
    EfiFileRead read;
    void *write;
    void *get_position;
    EfiFileSetPosition set_position;
    void *get_info;
    void *set_info;
    void *flush;
    void *open_ex;
    void *read_ex;
    void *write_ex;
    void *flush_ex;
};

typedef EFI_STATUS (*EfiOpenVolume)(void *, EfiFile **);
typedef struct {
    uint64_t revision;
    EfiOpenVolume open_volume;
} EfiSimpleFileSystem;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    uint32_t pixel_information[4];
    uint32_t pixels_per_scan_line;
} EfiGraphicsOutputModeInfo;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    EfiGraphicsOutputModeInfo *info;
    EFI_UINTN info_size;
    EFI_PHYSICAL_ADDRESS frame_buffer_base;
    EFI_UINTN frame_buffer_size;
} EfiGraphicsOutputMode;

typedef struct {
    void *query_mode;
    void *set_mode;
    void *blt;
    EfiGraphicsOutputMode *mode;
} EfiGraphicsOutput;

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} EfiMemoryDescriptor;

typedef struct {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} Elf64Header;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t align;
} Elf64ProgramHeader;

typedef void (*SysVKernelEntry)(const AgentOsBootInfoV2 *)
    __attribute__((sysv_abi));

static const EfiGuid loaded_image_guid = {
    0x5b1b31a1, 0x9562, 0x11d2,
    {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const EfiGuid simple_file_system_guid = {
    0x964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const EfiGuid graphics_output_guid = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};
static const EfiGuid acpi20_guid = {
    0x8868e871, 0xe4f1, 0x11d3,
    {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
};
static const EfiGuid acpi10_guid = {
    0xeb9d2d30, 0x2d88, 0x11d3,
    {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
};

static uint8_t kernel_file[UINT64_C(8) * 1024 * 1024]
    __attribute__((aligned(4096)));
static uint8_t initrd_file[UINT64_C(0x80000)]
    __attribute__((aligned(16)));
static uint8_t memory_map[UINT64_C(64) * 1024]
    __attribute__((aligned(16)));
/* The BIOS path places BootInfo below 2 MiB.  The first kernel page table
 * keeps that same identity window, so UEFI allocates this handoff block at a
 * stable low address before ExitBootServices rather than leaving it in the
 * firmware image's high loader allocation. */
static AgentOsE820EntryV2 *e820;
static AgentOsBootInfoV2 *boot_info;

static inline uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_puts(const char *text) {
    while (*text != '\0') {
        while ((io_in8(0x3fd) & 0x20u) == 0) { }
        io_out8(0x3f8, (uint8_t)*text++);
    }
}

static void console_puts(EfiSystemTable *system_table, const char *text) {
    if (system_table == 0 || system_table->con_out == 0 ||
        system_table->con_out->output_string == 0) {
        return;
    }
    CHAR16 line[128];
    uint32_t index = 0;
    while (text[index] != '\0' && index + 2 < sizeof(line) / sizeof(line[0])) {
        line[index] = (CHAR16)(uint8_t)text[index];
        ++index;
    }
    line[index++] = '\r';
    line[index++] = '\n';
    line[index] = 0;
    (void)system_table->con_out->output_string(system_table->con_out, line);
}

static void report(EfiSystemTable *system_table, const char *text) {
    serial_puts(text);
    serial_puts("\r\n");
    console_puts(system_table, text);
}

static void zero_bytes(void *destination, uint64_t length) {
    uint8_t *bytes = (uint8_t *)destination;
    for (uint64_t index = 0; index < length; ++index) {
        bytes[index] = 0;
    }
}

static void copy_bytes(void *destination, const void *source, uint64_t length) {
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    for (uint64_t index = 0; index < length; ++index) {
        out[index] = in[index];
    }
}

static int range_ok(uint64_t offset, uint64_t length, uint64_t total) {
    return offset <= total && length <= total - offset;
}

static int load_elf(EfiSystemTable *system_table, uint64_t file_size,
                    uint64_t *out_start, uint64_t *out_end,
                    uint64_t *out_entry) {
    if (file_size < sizeof(Elf64Header)) {
        return 0;
    }
    const Elf64Header *header = (const Elf64Header *)kernel_file;
    if (header->ident[0] != 0x7f || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != ELFCLASS64 || header->ident[5] != ELFDATA2LSB ||
        header->type != ET_EXEC || header->machine != EM_X86_64 ||
        header->phentsize != sizeof(Elf64ProgramHeader) || header->phnum == 0 ||
        !range_ok(header->phoff, (uint64_t)header->phnum * header->phentsize,
                  file_size)) {
        return 0;
    }
    const Elf64ProgramHeader *programs =
        (const Elf64ProgramHeader *)(kernel_file + header->phoff);
    uint64_t start = UINT64_MAX;
    uint64_t end = 0;
    uint32_t load_count = 0;
    for (uint16_t index = 0; index < header->phnum; ++index) {
        const Elf64ProgramHeader *program = &programs[index];
        if (program->type != PT_LOAD) {
            continue;
        }
        if (program->file_size > program->memory_size ||
            !range_ok(program->offset, program->file_size, file_size) ||
            program->virtual_address + program->memory_size <
                program->virtual_address ||
            program->virtual_address < UINT64_C(0x00100000) ||
            program->virtual_address + program->memory_size >
                UINT64_C(0x00200000) ||
            (program->virtual_address & 0xfff) != (program->offset & 0xfff)) {
            return 0;
        }
        uint64_t segment_start = program->virtual_address & ~UINT64_C(0xfff);
        uint64_t segment_end = (program->virtual_address +
                                program->memory_size + 0xfff) &
                               ~UINT64_C(0xfff);
        if (segment_end <= segment_start || segment_start < start ||
            (load_count != 0 && segment_start < end)) {
            if (load_count != 0 && segment_start < end) return 0;
        }
        if (segment_start < start) start = segment_start;
        if (segment_end > end) end = segment_end;
        ++load_count;
    }
    if (load_count == 0 || start != UINT64_C(0x00100000) ||
        header->entry < start || header->entry >= end) {
        return 0;
    }
    EFI_PHYSICAL_ADDRESS allocation = start;
    EfiBootServices *services = system_table->boot_services;
    EFI_STATUS status = services->allocate_pages(
        EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, (end - start) / 0x1000,
        &allocation);
    if (status != EFI_SUCCESS || allocation != start) {
        return 0;
    }
    zero_bytes((void *)(uintptr_t)start, end - start);
    for (uint16_t index = 0; index < header->phnum; ++index) {
        const Elf64ProgramHeader *program = &programs[index];
        if (program->type == PT_LOAD && program->file_size != 0) {
            copy_bytes((void *)(uintptr_t)program->virtual_address,
                       kernel_file + program->offset, program->file_size);
        }
    }
    *out_start = start;
    *out_end = end;
    *out_entry = header->entry;
    return 1;
}

static uint32_t collect_memory_map(EfiSystemTable *system_table,
                                   EFI_UINTN *out_key,
                                   EFI_UINTN *out_descriptor_size,
                                   uint32_t *out_version) {
    EfiBootServices *services = system_table->boot_services;
    EFI_UINTN map_size = sizeof(memory_map);
    EFI_UINTN key = 0;
    EFI_UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;
    EFI_STATUS status = services->get_memory_map(
        &map_size, memory_map, &key, &descriptor_size, &descriptor_version);
    if (status != EFI_SUCCESS || descriptor_size < sizeof(EfiMemoryDescriptor) ||
        descriptor_size > sizeof(memory_map) ||
        map_size / descriptor_size > 256) {
        return 0;
    }
    uint32_t count = (uint32_t)(map_size / descriptor_size);
    for (uint32_t index = 0; index < count; ++index) {
        const EfiMemoryDescriptor *source =
            (const EfiMemoryDescriptor *)(memory_map +
                                          (uint64_t)index * descriptor_size);
        AgentOsE820EntryV2 *destination = &e820[index];
        destination->base = source->physical_start;
        destination->length = source->number_of_pages * UINT64_C(0x1000);
        destination->type = source->type == 7 ? 1 : 2;
        destination->extended_attributes = 0;
    }
    *out_key = key;
    *out_descriptor_size = descriptor_size;
    *out_version = descriptor_version;
    return count;
}

static int guid_equal(const EfiGuid *left, const EfiGuid *right) {
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    for (uint32_t index = 0; index < sizeof(EfiGuid); ++index) {
        if (a[index] != b[index]) return 0;
    }
    return 1;
}

static void collect_platform_info(EfiSystemTable *system_table) {
    EfiConfigurationTable *tables =
        (EfiConfigurationTable *)system_table->configuration_table;
    for (uint32_t index = 0; index < system_table->number_of_table_entries;
         ++index) {
        if ((guid_equal(&tables[index].vendor_guid, &acpi20_guid) ||
             guid_equal(&tables[index].vendor_guid, &acpi10_guid)) &&
            boot_info->acpi_rsdp == 0) {
            boot_info->acpi_rsdp = (uint64_t)(uintptr_t)tables[index].vendor_table;
            boot_info->flags |= AGENT_OS_BOOT_V2_FLAG_ACPI;
        }
    }
    if (system_table->boot_services->locate_protocol != 0) {
        EfiGraphicsOutput *graphics = 0;
        if (system_table->boot_services->locate_protocol(
                (EfiGuid *)&graphics_output_guid, 0,
                (void **)&graphics) == EFI_SUCCESS &&
            graphics != 0 && graphics->mode != 0 && graphics->mode->info != 0) {
            boot_info->framebuffer_addr = graphics->mode->frame_buffer_base;
            boot_info->framebuffer_size = graphics->mode->frame_buffer_size;
            boot_info->framebuffer_width = graphics->mode->info->horizontal_resolution;
            boot_info->framebuffer_height = graphics->mode->info->vertical_resolution;
            boot_info->framebuffer_pitch = graphics->mode->info->pixels_per_scan_line * 4;
            boot_info->flags |= AGENT_OS_BOOT_V2_FLAG_FRAMEBUFFER;
        }
    }
}

static EFI_STATUS read_kernel(EfiSystemTable *system_table, EFI_HANDLE image,
                              uint64_t *out_size) {
    EfiBootServices *services = system_table->boot_services;
    EfiLoadedImage *loaded = 0;
    EFI_STATUS status = services->handle_protocol(
        image, (EfiGuid *)&loaded_image_guid, (void **)&loaded);
    if (status != EFI_SUCCESS || loaded == 0) return status;
    EfiSimpleFileSystem *filesystem = 0;
    status = services->handle_protocol(
        loaded->device_handle, (EfiGuid *)&simple_file_system_guid,
        (void **)&filesystem);
    if (status != EFI_SUCCESS || filesystem == 0) return status;
    EfiFile *root = 0;
    status = filesystem->open_volume(filesystem, &root);
    if (status != EFI_SUCCESS || root == 0) return status;
    static CHAR16 path[] = {'\\','A','G','E','N','T','O','S','\\',
                            'K','E','R','N','E','L','.','E','L','F',0};
    EfiFile *kernel = 0;
    status = root->open(root, &kernel, path, EFI_FILE_MODE_READ, 0);
    (void)root->close(root);
    if (status != EFI_SUCCESS || kernel == 0) return status;
    EFI_UINTN size = sizeof(kernel_file);
    status = kernel->read(kernel, &size, kernel_file);
    (void)kernel->close(kernel);
    if (status != EFI_SUCCESS || size == 0 || size > sizeof(kernel_file)) {
        return EFI_LOAD_ERROR;
    }
    *out_size = size;
    return EFI_SUCCESS;
}

static EFI_STATUS read_initrd(EfiSystemTable *system_table, EFI_HANDLE image,
                              uint64_t *out_size) {
    EfiBootServices *services = system_table->boot_services;
    EfiLoadedImage *loaded = 0;
    EFI_STATUS status = services->handle_protocol(
        image, (EfiGuid *)&loaded_image_guid, (void **)&loaded);
    if (status != EFI_SUCCESS || loaded == 0) return status;
    EfiSimpleFileSystem *filesystem = 0;
    status = services->handle_protocol(
        loaded->device_handle, (EfiGuid *)&simple_file_system_guid,
        (void **)&filesystem);
    if (status != EFI_SUCCESS || filesystem == 0) return status;
    EfiFile *root = 0;
    status = filesystem->open_volume(filesystem, &root);
    if (status != EFI_SUCCESS || root == 0) return status;
    static CHAR16 path[] = {'\\','A','G','E','N','T','O','S','\\',
                            'I','N','I','T','R','D','.','B','I','N',0};
    EfiFile *file = 0;
    status = root->open(root, &file, path, EFI_FILE_MODE_READ, 0);
    (void)root->close(root);
    if (status != EFI_SUCCESS || file == 0) return status;
    EFI_UINTN size = sizeof(initrd_file);
    status = file->read(file, &size, initrd_file);
    (void)file->close(file);
    if (status != EFI_SUCCESS || size == 0 || size > sizeof(initrd_file)) {
        return EFI_LOAD_ERROR;
    }
    *out_size = size;
    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE image, EfiSystemTable *system_table) {
    if (system_table == 0 || system_table->boot_services == 0) {
        return EFI_INVALID_PARAMETER;
    }
    report(system_table, "AGENTOS UEFI LOADER");
    uint64_t file_size = 0;
    if (read_kernel(system_table, image, &file_size) != EFI_SUCCESS) {
        report(system_table, "AGENTOS UEFI KERNEL READ FAIL");
        return EFI_LOAD_ERROR;
    }
    uint64_t kernel_start = 0;
    uint64_t kernel_end = 0;
    uint64_t entry = 0;
    if (!load_elf(system_table, file_size, &kernel_start, &kernel_end, &entry)) {
        report(system_table, "AGENTOS UEFI ELF REJECT");
        return EFI_LOAD_ERROR;
    }
    report(system_table, "AGENTOS UEFI ELF OK");

    uint64_t initrd_size = 0;
    int have_initrd =
        read_initrd(system_table, image, &initrd_size) == EFI_SUCCESS;
    EFI_PHYSICAL_ADDRESS initrd_start = UINT64_C(0x00030000);
    if (have_initrd) {
        if (initrd_size > AGENT_OS_INITRD_MAX_SIZE ||
            system_table->boot_services->allocate_pages(
                EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA,
                (initrd_size + 0xfff) / 0x1000, &initrd_start) != EFI_SUCCESS ||
            initrd_start != UINT64_C(0x00030000)) {
            report(system_table, "AGENTOS UEFI INITRD ALLOC FAIL");
            return EFI_LOAD_ERROR;
        }
        copy_bytes((void *)(uintptr_t)initrd_start, initrd_file, initrd_size);
        report(system_table, "AGENTOS UEFI INITRD OK");
    } else {
        report(system_table, "AGENTOS UEFI INITRD OPTIONAL ABSENT");
    }

    EFI_UINTN map_key = 0;
    EFI_UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;
    uint32_t map_count;
    EFI_PHYSICAL_ADDRESS handoff = UINT64_C(0x00080000);
    EFI_STATUS handoff_status = system_table->boot_services->allocate_pages(
        EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, 16, &handoff);
    if (handoff_status != EFI_SUCCESS || handoff != UINT64_C(0x00080000)) {
        report(system_table, "AGENTOS UEFI HANDOFF ALLOC FAIL");
        return EFI_LOAD_ERROR;
    }
    boot_info = (AgentOsBootInfoV2 *)(uintptr_t)handoff;
    e820 = (AgentOsE820EntryV2 *)(uintptr_t)(handoff + 0x1000);
    zero_bytes(boot_info, 16 * 0x1000);
    map_count = collect_memory_map(
        system_table, &map_key, &descriptor_size, &descriptor_version);
    if (map_count == 0) {
        report(system_table, "AGENTOS UEFI MEMORY MAP FAIL");
        return EFI_LOAD_ERROR;
    }
    boot_info->magic = AGENT_OS_BOOT_INFO_MAGIC_V2;
    boot_info->version = AGENT_OS_BOOT_INFO_VERSION_V2;
    boot_info->size = sizeof(*boot_info);
    boot_info->flags = AGENT_OS_BOOT_V2_FLAG_IDENTITY_MAP |
                      AGENT_OS_BOOT_V2_FLAG_UEFI_MEMORY_MAP;
    boot_info->memory_map_addr = (uint64_t)(uintptr_t)e820;
    boot_info->memory_map_count = map_count;
    boot_info->memory_map_entry_size = sizeof(e820[0]);
    boot_info->loader_type = AGENT_OS_BOOT_LOADER_UEFI;
    boot_info->kernel_phys_start = kernel_start;
    boot_info->kernel_phys_end = kernel_end;
    boot_info->staging_phys_start = (uint64_t)(uintptr_t)kernel_file;
    boot_info->staging_phys_end = boot_info->staging_phys_start + file_size;
    if (have_initrd) {
        boot_info->flags |= AGENT_OS_BOOT_V2_FLAG_INITRD;
        boot_info->initrd_phys_start = initrd_start;
        boot_info->initrd_phys_end = initrd_start + initrd_size;
    }
    collect_platform_info(system_table);
    report(system_table, "AGENTOS UEFI EXIT BOOT SERVICES");
#ifdef AGENT_OS_TEST_FORCE_MAP_KEY_FAILURE
    /* Fault-injection build only: prove that a stale key is recovered by
     * collecting a fresh map and retrying before entering the kernel. */
    EFI_UINTN injected_map_key = map_key + 1;
    EFI_STATUS status = system_table->boot_services->exit_boot_services(
        image, injected_map_key);
#else
    EFI_STATUS status = system_table->boot_services->exit_boot_services(
        image, map_key);
#endif
    if (status != EFI_SUCCESS) {
        map_count = collect_memory_map(
            system_table, &map_key, &descriptor_size, &descriptor_version);
        if (map_count == 0) return status;
        boot_info->memory_map_count = map_count;
        status = system_table->boot_services->exit_boot_services(image, map_key);
    }
    if (status != EFI_SUCCESS) {
        report(system_table, "AGENTOS UEFI EXIT FAIL");
        return status;
    }
    ((SysVKernelEntry)(uintptr_t)entry)(boot_info);
    for (;;) __asm__ volatile ("hlt");
    return EFI_SUCCESS;
}

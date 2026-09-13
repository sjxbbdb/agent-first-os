# G5 UEFI kernel handoff evidence

The full UEFI vertical slice uses the self-written
`product/kernel/arch/x86_64/boot/uefi/uefi_loader.c` and the same kernel ELF
produced by the BIOS build. It opens `AGENTOS/KERNEL.ELF` through the UEFI file
system protocol, validates the x86_64 `PT_LOAD` subset, allocates the linked
1 MiB image, constructs a low identity-mapped `BootInfo v2`, converts the UEFI
memory map to the existing bounded descriptor format, calls
`ExitBootServices`, and invokes the shared `kernel_entry` using the SysV ABI.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g5-uefi-kernel-test.sh'
PASS: OVMF UEFI loader reads ELF, exits boot services, enters the shared kernel_entry, and runs Ring 3
```

The QEMU log also contains `AGENTOS UEFI ELF OK`,
`AGENTOS UEFI EXIT BOOT SERVICES`, `boot_info.loader_type=...0002`,
`G2 ELF user load OK`, and `SCHEDULER idle - all tasks exited`. This is real
OVMF execution, not a host-only build check.

The QEMU full test adds a VGA device and checks BootInfo flags `0x1d`, covering
identity mapping, UEFI memory map, GOP framebuffer and ACPI RSDP discovery.
The remaining G5 boundary is explicit: the loader currently accepts only the
linked low-memory kernel layout, does not pass an initrd, and the malformed-ELF
and forced map-key-change matrices still need dedicated negative fixtures. The
malformed-ELF portion is now covered by
`product/tests/g5-uefi-negative-test.sh`, which reaches `AGENTOS UEFI ELF
REJECT` and never prints the kernel marker; forced map-key injection remains.

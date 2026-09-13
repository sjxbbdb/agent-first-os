# G5 UEFI Stage0 evidence

`product/tools/build-uefi.sh` uses the installed Clang PE/COFF target and
`lld-link` to build `BOOTX64.EFI` from the freestanding `uefi_stage0.c` entry.
The script places it at `EFI/BOOT/BOOTX64.EFI` in a FAT directory ESP.

Verification:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g5-uefi-stage0-test.sh'
PASS: OVMF loaded BOOTX64.EFI and reached UEFI stage0 marker
```

The test boots OVMF with the generated ESP and checks both the serial marker
`AGENTOS_UEFI_STAGE0` and the UEFI console marker. This proves the PE/COFF
entry and FAT ESP discovery path only. It does not claim ELF loading,
BootInfo v2 construction, `ExitBootServices`, GOP/ACPI collection, or the
shared `kernel_entry` handoff; those remain the unfinished G5 work.

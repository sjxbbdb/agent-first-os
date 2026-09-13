#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="${UEFI_BUILD_DIR:-$repo_root/build/uefi}"
esp_dir="$out_dir/esp"
mkdir -p "$out_dir" "$esp_dir/EFI/BOOT"
source_file="${UEFI_LOADER_SOURCE:-$repo_root/product/kernel/arch/x86_64/boot/uefi/uefi_stage0.c}"
object_file="$out_dir/uefi_loader.obj"
extra_cflags=()
if [[ -n "${UEFI_CFLAGS_EXTRA:-}" ]]; then
  # This variable is intentionally a test/build hook; callers must pass one
  # quoted flag string, for example -DAGENT_OS_TEST_FORCE_MAP_KEY_FAILURE.
  read -r -a extra_cflags <<< "$UEFI_CFLAGS_EXTRA"
fi
clang --target=x86_64-pc-windows-msvc \
  -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables \
  -fno-builtin -mno-red-zone \
  "${extra_cflags[@]}" \
  -I"$repo_root/product/kernel/include" \
  -c "$source_file" -o "$object_file"
lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib \
  /machine:x64 /out:"$out_dir/BOOTX64.EFI" "$object_file"
cp "$out_dir/BOOTX64.EFI" "$esp_dir/EFI/BOOT/BOOTX64.EFI"
if [[ -n "${UEFI_KERNEL_ELF:-}" ]]; then
  [[ -f "$UEFI_KERNEL_ELF" ]] || { echo "UEFI_KERNEL_ELF not found: $UEFI_KERNEL_ELF" >&2; exit 1; }
  mkdir -p "$esp_dir/AGENTOS"
  cp "$UEFI_KERNEL_ELF" "$esp_dir/AGENTOS/KERNEL.ELF"
fi
if [[ -n "${UEFI_INITRD_BIN:-}" ]]; then
  [[ -f "$UEFI_INITRD_BIN" ]] || { echo "UEFI_INITRD_BIN not found: $UEFI_INITRD_BIN" >&2; exit 1; }
  mkdir -p "$esp_dir/AGENTOS"
  cp "$UEFI_INITRD_BIN" "$esp_dir/AGENTOS/INITRD.BIN"
fi
file "$out_dir/BOOTX64.EFI"
echo "UEFI artifact: $out_dir/BOOTX64.EFI (source=$source_file)"
echo "UEFI ESP directory: $esp_dir"

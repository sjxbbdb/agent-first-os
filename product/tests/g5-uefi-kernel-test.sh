#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/build/uefi-full"
esp_dir="$out_dir/esp"
log="$out_dir/qemu-kernel.log"
mkdir -p "$out_dir"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
UEFI_BUILD_DIR="$out_dir" \
UEFI_LOADER_SOURCE="$repo_root/product/kernel/arch/x86_64/boot/uefi/uefi_loader.c" \
UEFI_KERNEL_ELF="$repo_root/build/bios/kernel.elf" \
    bash "$repo_root/product/tools/build-uefi.sh" > "$out_dir/build.log"
cp /usr/share/OVMF/OVMF_VARS_4M.fd "$out_dir/OVMF_VARS.fd"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 12s "$log" \
    -machine q35 -m 128M -nodefaults -device VGA \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file="$out_dir/OVMF_VARS.fd" \
    -drive format=raw,file=fat:rw:"$esp_dir" \
    -serial stdio -display none -no-reboot
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
grep -Fq 'AGENTOS UEFI ELF OK' "$log"
grep -Fq 'AGENTOS UEFI EXIT BOOT SERVICES' "$log"
grep -Fq 'boot_info.loader_type=0x0000000000000002' "$log"
grep -Fq 'flags=0x000000000000001d' "$log"
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: OVMF UEFI loader reads ELF, exits boot services, enters the shared kernel_entry, and runs Ring 3'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/build/uefi-semantic"
esp_dir="$out_dir/esp"
log="$out_dir/qemu-semantic.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G8_NATIVE \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
UEFI_BUILD_DIR="$out_dir" \
UEFI_LOADER_SOURCE="$repo_root/product/kernel/arch/x86_64/boot/uefi/uefi_loader.c" \
UEFI_KERNEL_ELF="$repo_root/build/bios/kernel.elf" \
    bash "$repo_root/product/tools/build-uefi.sh" >/dev/null
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
grep -Fq 'boot_info.loader_type=0x0000000000000002' "$log"
grep -Fq 'NATIVE REGISTRY CONTEXT TRUSTED INPUT OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: OVMF shared kernel_entry runs the native Ring 3 semantic/context slice'

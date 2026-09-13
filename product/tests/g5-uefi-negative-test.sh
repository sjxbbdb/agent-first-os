#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/build/uefi-negative"
esp_dir="$out_dir/esp"
log="$out_dir/qemu-negative.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
UEFI_BUILD_DIR="$out_dir" \
UEFI_LOADER_SOURCE="$repo_root/product/kernel/arch/x86_64/boot/uefi/uefi_loader.c" \
UEFI_KERNEL_ELF="$repo_root/build/bios/kernel.elf" \
    bash "$repo_root/product/tools/build-uefi.sh" >/dev/null
python3 - "$esp_dir/AGENTOS/KERNEL.ELF" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
data[0] = 0
path.write_bytes(data)
PY
cp /usr/share/OVMF/OVMF_VARS_4M.fd "$out_dir/OVMF_VARS.fd"

set +e
"$repo_root/product/tests/qemu-timeout.sh" 8s "$log" \
    -machine q35 -m 128M -nodefaults \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file="$out_dir/OVMF_VARS.fd" \
    -drive format=raw,file=fat:rw:"$esp_dir" \
    -serial stdio -display none -no-reboot
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
grep -Fq 'AGENTOS UEFI ELF REJECT' "$log"
! grep -Fq 'VM G1 4K NX map OK' "$log"
echo 'PASS: OVMF UEFI loader rejects a malformed kernel ELF before ExitBootServices or kernel_entry'

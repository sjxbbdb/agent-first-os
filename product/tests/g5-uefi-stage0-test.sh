#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/build/uefi"
log="$out_dir/qemu-stage0.log"
bash "$repo_root/product/tools/build-uefi.sh" > "$out_dir/build-stage0.log"
cp /usr/share/OVMF/OVMF_VARS_4M.fd "$out_dir/OVMF_VARS.fd"
set +e
"$repo_root/product/tests/qemu-timeout.sh" 8s "$log" \
  -machine q35 -nodefaults -m 128M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file="$out_dir/OVMF_VARS.fd" \
  -drive format=raw,file=fat:rw:"$out_dir/esp" \
  -serial stdio -display none -no-reboot
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
grep -Fq 'AGENTOS_UEFI_STAGE0' "$log"
grep -Fq 'AGENTOS UEFI STAGE0' "$log"
echo 'PASS: OVMF loaded BOOTX64.EFI and reached UEFI stage0 marker'

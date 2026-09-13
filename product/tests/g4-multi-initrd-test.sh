#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios-multi-initrd"
log="$build_dir/qemu-multi-initrd.log"
mkdir -p "$build_dir"

INITRD_MANIFEST="$repo_root/product/services/examples/multi.manifest.json" \
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 6s "$log" \
    -machine pc \
    -drive "format=raw,file=$repo_root/build/bios/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e

[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
grep -Fq 'G4 initrd manifest OK' "$log"
grep -Fq 'G4 Supervisor spawned service from initrd' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
echo 'PASS: versioned multi-service initrd table is validated and the first service is launched fail-closed'

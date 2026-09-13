#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g1-high-half"
mkdir -p "$build_dir"
gcc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/product/kernel/include" \
    "$repo_root/product/kernel/src/address_space.c" \
    "$repo_root/product/tests/g1_high_half_host.c" \
    -o "$build_dir/high-half-host"
"$build_dir/high-half-host"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
bash "$repo_root/product/tools/run-bios.sh" > "$repo_root/build/bios/qemu-g1-high-half.log" 2>&1
grep -Fq 'HIGH HALF alias contract READY low-exec preserved' \
    "$repo_root/build/bios/qemu-g1-high-half.log"
grep -Fq 'HIGH HALF RIP probe OK' "$repo_root/build/bios/qemu-g1-high-half.log"
grep -Fq 'G2 CR3 switch OK' "$repo_root/build/bios/qemu-g1-high-half.log"
echo 'PASS: host alias contract and BIOS/QEMU kernel high-half preparation'

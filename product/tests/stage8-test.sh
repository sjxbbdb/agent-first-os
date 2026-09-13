#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 bash "$repo_root/product/tools/build-bios.sh" >/dev/null
sync
# Reuse the canonical runner for the normal path; it keeps the QEMU process
# attached to tee and avoids a drvfs timing race seen with a freshly redirected
# stdout during the first user transition.
bash "$repo_root/product/tools/run-bios.sh" > "$build_dir/qemu-g1-isolation.log" 2>&1
grep -Fq 'USER RING3 OK' "$build_dir/qemu-g1-isolation.log"
grep -Fq 'boot_info.version=0x0000000000000002' "$build_dir/qemu-g1-isolation.log"
grep -Fq 'boot_info.loader_type=0x0000000000000001' "$build_dir/qemu-g1-isolation.log"
grep -Fq 'SYSCALL exit OK - user reclaimed' "$build_dir/qemu-g1-isolation.log"

# A Ring 3 read of the kernel text at 0x100000 must be a protection fault.
# This failed under the former shared user 2 MiB PDE and exercises the CPU's
# actual page permissions instead of only checking assembly source text.
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA=-dAGENT_OS_TEST_USER_KERNEL_READ \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
sync
bash "$repo_root/product/tools/run-bios.sh" > "$build_dir/qemu-g1-kernel-read-fault.log" 2>&1
grep -Fq 'vector=0x000000000000000e' "$build_dir/qemu-g1-kernel-read-fault.log"
grep -Fq 'error=0x0000000000000005' "$build_dir/qemu-g1-kernel-read-fault.log"
if grep -Fq 'USER RING3 OK' "$build_dir/qemu-g1-kernel-read-fault.log"; then
    echo 'FAIL: Ring 3 continued after reading a supervisor page' >&2
    exit 1
fi
echo 'PASS: BootInfo v2 boots; Ring 3 works; kernel-page read raises user protection fault'

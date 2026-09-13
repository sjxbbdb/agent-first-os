#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

run_probe() {
    local macro="$1" expected_error="$2" expected_cr2="$3" name="$4"
    KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
    USER_NASMFLAGS_EXTRA="-d$macro" \
        bash "$repo_root/product/tools/build-bios.sh" >/dev/null
    sync
    bash "$repo_root/product/tools/run-bios.sh" > "$build_dir/qemu-g1-$name.log" 2>&1
    grep -Fq 'vector=0x000000000000000e' "$build_dir/qemu-g1-$name.log"
    grep -Fq "error=0x$expected_error" "$build_dir/qemu-g1-$name.log"
    grep -Fq "cr2=0x$expected_cr2" "$build_dir/qemu-g1-$name.log"
    if grep -Fq 'USER RING3 OK' "$build_dir/qemu-g1-$name.log"; then
        echo "FAIL: $name continued after a protection fault" >&2
        exit 1
    fi
}

run_probe AGENT_OS_TEST_USER_KERNEL_WRITE 0000000000000007 0000000000100000 kernel-write
run_probe AGENT_OS_TEST_USER_TEXT_WRITE 0000000000000007 0000000000400042 text-write
run_probe AGENT_OS_TEST_NX_STACK 0000000000000015 00000000001feff8 nx-stack
echo 'PASS: Ring 3 kernel write, user text write, and NX stack execute are rejected'

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
bash "$repo_root/product/tools/build-bios.sh" >/dev/null
readelf -S "$build_dir/kernel.elf" | grep -Fq '.user_text'
readelf -l "$build_dir/kernel.elf" | grep -Fq '0x0000000000180000'
bash "$repo_root/product/tools/run-bios.sh" > "$build_dir/qemu-m7-gated.log"
grep -Fq 'RING3 ABI READY - launch gated pending TSS' "$build_dir/qemu-m7-gated.log"
grep -Fq 'IDT OK - 256 vectors' "$build_dir/qemu-m7-gated.log"
grep -Fq 'KERNEL C OK' "$build_dir/qemu-m7-gated.log"

set +e
KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 bash "$repo_root/product/tools/build-bios.sh" >/dev/null
"$repo_root/product/tests/qemu-timeout.sh" 3s "$build_dir/qemu-m7-ring3.log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
if [[ "$qemu_status" -ne 124 ]]; then
    echo "unexpected QEMU status: $qemu_status" >&2
    exit 1
fi
grep -Fq 'RING3 launch' "$build_dir/qemu-m7-ring3.log"
grep -Fq 'USER RING3 OK' "$build_dir/qemu-m7-ring3.log"
grep -Fq 'SYSCALL write OK' "$build_dir/qemu-m7-ring3.log"
grep -Fq 'SYSCALL write EFAULT' "$build_dir/qemu-m7-ring3.log"
grep -Fq 'SYSCALL denied - unknown number' "$build_dir/qemu-m7-ring3.log"
grep -Fq 'SYSCALL exit OK - user reclaimed' "$build_dir/qemu-m7-ring3.log"
echo 'PASS: Ring 3 enters through TSS/RSP0, exercises int 0x80 validation, and exits'

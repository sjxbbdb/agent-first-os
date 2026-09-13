#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g2-irq.log"
KERNEL_CFLAGS_EXTRA='-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_TIMER_PREEMPT' \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_IRQ_WAIT \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none \
    -d int -D "$build_dir/qemu-g2-irq-trace.log"
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]] || { cat "$log"; exit 1; }
if ! grep -Fq 'SCHEDULER idle - all tasks exited' "$log" || \
   grep -Fq 'EXCEPTION observed' "$log"; then
    cat "$log"
    echo 'FAIL: user execution did not survive a legacy timer period' >&2
    exit 1
fi
grep -Fq 'TIMER PREEMPT switched task' "$log"
echo 'PASS: Ring 3 remains runnable across a legacy timer period'

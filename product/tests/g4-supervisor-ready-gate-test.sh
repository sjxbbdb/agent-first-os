#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g4-supervisor-ready-gate.log"

KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_RING3 \
USER_NASMFLAGS_EXTRA='-DAGENT_OS_TEST_SUPERVISOR -DAGENT_OS_TEST_SUPERVISOR_READY_GATE' \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null

set +e
"$repo_root/product/tests/qemu-timeout.sh" 4s "$log" \
    -machine pc \
    -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
    -boot c -display none -serial stdio -monitor none
qemu_status=$?
set -e
[[ "$qemu_status" -eq 124 ]]
grep -Fq 'SERVICE WAITING DEPENDENCY' "$log"
grep -Fq 'SUPERVISOR READY GATE SENT' "$log"
grep -Fq 'SERVICE DEPENDENCY READY' "$log"
grep -Fq 'SUPERVISOR RESTART HEARTBEAT 2' "$log"
grep -Fq 'SYSCALL wait OK - child reaped' "$log"
python3 - "$log" <<'PY'
from pathlib import Path
import sys
lines = Path(sys.argv[1]).read_text().splitlines()
pos = {marker: next(i for i, line in enumerate(lines) if marker in line)
       for marker in ("SERVICE WAITING DEPENDENCY", "SUPERVISOR READY GATE SENT", "SERVICE DEPENDENCY READY")}
assert pos["SERVICE WAITING DEPENDENCY"] < pos["SUPERVISOR READY GATE SENT"] < pos["SERVICE DEPENDENCY READY"]
PY
echo 'PASS: native Supervisor releases a dependent Ring 3 service only after READY'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
data_disk="$build_dir/g7-native-journal-data.img"
prepare_log="$build_dir/qemu-g7-native-journal-prepare.log"
recover_log="$build_dir/qemu-g7-native-journal-recover.log"
qmp="/tmp/agent-os-g7-journal.qmp"

rm -f "$qmp" "$prepare_log" "$recover_log"
KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G7_NATIVE_JOURNAL_PREPARE" \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G7_NATIVE_JOURNAL_PREPARE \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
cp "$build_dir/stage1.img" "$data_disk"
truncate -s 4M "$data_disk"
dd if=/dev/zero of="$data_disk" bs=512 count=3 seek=1 conv=notrunc status=none
printf 'OLDVAL01' | dd of="$data_disk" bs=1 seek=1536 conv=notrunc status=none

# Stop immediately after PREPARE and payload have each reached a completed
# flush. This is a controlled crash window, not a physical power-loss claim.
qemu-system-x86_64 \
  -machine pc \
  -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -drive "if=none,id=vd0,format=raw,file=$data_disk,cache=directsync" \
  -device virtio-blk-pci,drive=vd0 \
  -boot c -display none -serial "file:$prepare_log" -monitor none \
  -qmp "unix:$qmp,server=on,wait=off" &
qemu_pid=$!
prepare_seen=0
for _ in $(seq 1 120); do
  if grep -Fq 'JOURNAL PREPARE DURABLE' "$prepare_log" 2>/dev/null; then
    prepare_seen=1
    break
  fi
  sleep 0.05
done
if [[ "$prepare_seen" -ne 1 ]]; then
  kill "$qemu_pid" 2>/dev/null || true
  wait "$qemu_pid" 2>/dev/null || true
  cat "$prepare_log" >&2 || true
  echo 'journal prepare marker was not observed' >&2
  exit 1
fi
python3 - "$qmp" <<'PY'
import json, socket, sys
path = sys.argv[1]
s = socket.socket(socket.AF_UNIX)
s.settimeout(1.0)
s.connect(path)
try:
    s.recv(4096)
except TimeoutError:
    pass
s.sendall((json.dumps({"execute": "qmp_capabilities"}) + "\r\n").encode())
try:
    s.recv(4096)
except TimeoutError:
    pass
s.sendall((json.dumps({"execute": "quit"}) + "\r\n").encode())
try:
    s.recv(4096)
except TimeoutError:
    pass
s.close()
PY
for _ in $(seq 1 50); do
  kill -0 "$qemu_pid" 2>/dev/null || break
  sleep 0.1
done
if kill -0 "$qemu_pid" 2>/dev/null; then
  kill "$qemu_pid" 2>/dev/null || true
fi
wait "$qemu_pid" 2>/dev/null || true
rm -f "$qmp"
grep -Fq 'JOURNAL PREPARE DURABLE' "$prepare_log"

KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G7_NATIVE_JOURNAL_RECOVER" \
USER_NASMFLAGS_EXTRA=-DAGENT_OS_TEST_G7_NATIVE_JOURNAL_RECOVER \
    bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$recover_log" \
  -machine pc \
  -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -drive "if=none,id=vd0,format=raw,file=$data_disk,cache=directsync" \
  -device virtio-blk-pci,drive=vd0 \
  -boot c -display none -serial stdio -monitor none
recover_status=$?
set -e
if [[ "$recover_status" -ne 124 ]]; then
  echo "unexpected recovery QEMU status: $recover_status" >&2
  cat "$recover_log" >&2 || true
  exit 1
fi
grep -Fq 'JOURNAL RECOVERY ROLLBACK' "$recover_log"
grep -Fq 'SYSCALL block read OK used completion' "$recover_log"
grep -Fq 'SYSCALL block write OK used completion' "$recover_log"
grep -Fq 'SYSCALL block flush OK used completion' "$recover_log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$recover_log"
python3 - "$data_disk" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
if data[512:520] != b'ROLLBK01':
    raise SystemExit('sector 1 does not contain the rollback record')
if data[1536:1544] != b'OLDVAL01':
    raise SystemExit('payload sector was not restored after PREPARE recovery')
PY
echo 'PASS: BIOS/QEMU Ring 3 journal PREPARE crash window flushes, detects and rolls back the payload across restart'

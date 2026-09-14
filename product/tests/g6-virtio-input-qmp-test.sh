#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-virtio-input-qmp.log"
qmp="/tmp/agent-os-g6-input-qmp.sock"
rm -f "$qmp" "$log"
KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G6_VIRTIO_INPUT" \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null

qemu-system-x86_64 \
  -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -device virtio-keyboard-pci -qmp "unix:$qmp,server=on,wait=off" \
  -boot c -display none -serial "file:$log" -monitor none \
  >/dev/null 2>"$log.stderr" &
qemu_pid=$!
cleanup() { kill "$qemu_pid" 2>/dev/null || true; rm -f "$qmp"; }
trap cleanup EXIT HUP INT TERM

for attempt in $(seq 1 50); do
  [[ -S "$qmp" ]] && break
  sleep 0.1
done
[[ -S "$qmp" ]]
# The guest's bounded poll can finish in a few milliseconds. Send a short,
# deterministic burst so at least one event overlaps queue arming without
# claiming success until the guest reports a used-ring completion.
python3 -c 'import json,socket,sys,time; p=sys.argv[1]; s=socket.socket(socket.AF_UNIX); s.connect(p); s.recv(4096); s.sendall((json.dumps({"execute":"qmp_capabilities"})+"\r\n").encode()); s.recv(4096); e={"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"down":True,"key":{"type":"qcode","data":"a"}}},{"type":"key","data":{"down":False,"key":{"type":"qcode","data":"a"}}}]}}; out=[]
for _ in range(50):
 s.sendall((json.dumps(e)+"\r\n").encode()); s.settimeout(0.2)
 try: out.append(s.recv(4096).decode())
 except TimeoutError: pass
 time.sleep(0.02)
sys.stdout.write("".join(out)); s.close()' "$qmp" >"$build_dir/g6-input-qmp-response.log"
sleep 2
grep -Fq 'G6 virtio input' "$log" || true
if grep -Fq 'G6 virtio input EVENT OK' "$log"; then
  echo 'QMP synthetic input reached the guest virtio-input completion path'
else
  echo 'BLOCKED: QMP synthetic input was sent, but no guest virtio-input completion was observed'
  exit 2
fi

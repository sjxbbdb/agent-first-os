#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
log="$build_dir/qemu-g6-virtio-net-rx.log"
KERNEL_CFLAGS_EXTRA="-DAGENT_OS_TEST_RING3 -DAGENT_OS_TEST_G6_VIRTIO_NET_RX" \
  bash "$repo_root/product/tools/build-bios.sh" >/dev/null
set +e
"$repo_root/product/tests/qemu-timeout.sh" 5s "$log" \
  -machine pc -drive "format=raw,file=$build_dir/stage1.img,if=ide" \
  -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
  -boot c -display none -serial stdio -monitor none
status=$?
set -e
[[ "$status" -eq 124 ]]
grep -Fq 'G6 virtio net transport READY' "$log"
grep -Fq 'G6 virtio net RX ARMED' "$log"
grep -Fq 'G2 ELF user load OK' "$log"
grep -Fq 'SCHEDULER idle - all tasks exited' "$log"
if grep -Fq 'G6 virtio net RX OK' "$log"; then
  echo 'PASS: real QEMU virtio-net RX queue completed a received packet'
else
  grep -Fq 'G6 virtio net RX WAIT' "$log"
  echo 'BLOCKED: real QEMU virtio-net RX queue was armed, but no packet entered the user backend'
  exit 2
fi

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source="$repo_root/product/kernel/src/virtio_pci.c"
header="$repo_root/product/kernel/include/virtio_pci.h"
write_body="$(sed -n '/int agent_os_virtio_block_write_sector/,/^int agent_os_virtio_probe_net/p' "$source")"

# Compile the actual Ring-0 transport source with the production freestanding
# flags.  This is a syntax/ABI contract only; it does not execute I/O.
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
  -I"$repo_root/product/kernel/include" -fsyntax-only "$source"

grep -Fq 'agent_os_virtio_block_write_sector' "$header"
grep -Fq 'request->type = 1' <<<"$write_body"
grep -Fq 'sizeof(request->data)' <<<"$write_body"
grep -Fq 'data == 0' <<<"$write_body"
grep -Fq 'probe->queue_ready' <<<"$write_body"

# Keep the boot sector outside the journal contract.  The caller must select a
# data sector (sector 1 or later) and the future syscall/IPC adapter owns that
# policy check; this source remains hardware-only.
if grep -Fq 'request->sector = 0;' <<<"$write_body"; then
  echo 'FAIL: write primitive hard-codes boot sector 0' >&2
  exit 1
fi

printf 'PASS: native journal block write compile/negative contract (no persistence claim)\n'

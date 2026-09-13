#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$repo_root/build"
tmp_c="$repo_root/build/g6-virtio-input-bar-contract.c"
tmp_bin="$repo_root/build/g6-virtio-input-bar-contract"
python3 -c 'from pathlib import Path; import sys; Path(sys.argv[1]).write_text("#include <stdint.h>\n#include <assert.h>\n#include \"virtio_pci.h\"\nint main(void) { assert(agent_os_virtio_modern_window_ok(0x1000, 0x100, 0x200000)); assert(!agent_os_virtio_modern_window_ok(0x1ffff0, 0x100, 0x200000)); assert(!agent_os_virtio_modern_window_ok(UINT64_MAX - 3, 8, UINT64_MAX)); return 0; }\n")' "$tmp_c"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" "$tmp_c" -o "$tmp_bin"
"$tmp_bin"
echo 'PASS: modern virtio BAR identity-map boundary contract'

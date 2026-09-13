#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$repo_root/build/g6-virtio-modern-caps-host"
gcc -std=c11 -Wall -Wextra -Werror "$repo_root/product/tests/g6_virtio_modern_caps_host.c" -o "$bin"
"$bin"
echo 'PASS: modern virtio capability layout parser rejects malformed chains and overflow'

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$repo_root/build/g6-mmio-map-host"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" \
    "$repo_root/product/tests/g6-mmio-map-test.c" \
    "$repo_root/product/kernel/src/address_space.c" \
    "$repo_root/product/kernel/src/physmem.c" -o "$bin"
"$bin"
echo 'PASS: bounded MMIO identity mapping preserves prior entries and rejects alias/overflow/boundary cases'

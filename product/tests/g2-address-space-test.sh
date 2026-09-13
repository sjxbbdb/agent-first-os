#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g2"
mkdir -p "$build_dir"
gcc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/product/kernel/include" \
    "$repo_root/product/kernel/src/address_space.c" \
    "$repo_root/product/tests/g2_address_space_host.c" \
    -o "$build_dir/address-space-host"
"$build_dir/address-space-host"

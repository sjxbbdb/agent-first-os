#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g2"
mkdir -p "$build_dir"
cc_flags=(-std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include")
gcc "${cc_flags[@]}" "$repo_root/product/kernel/src/physmem.c" \
    "$repo_root/product/tests/g2_physical_allocator_host.c" \
    -o "$build_dir/physical-allocator-host"
"$build_dir/physical-allocator-host"

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g3"
mkdir -p "$build_dir"
gcc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/product/kernel/include" \
    "$repo_root/product/kernel/src/shared_memory.c" \
    "$repo_root/product/tests/g3_shm_host.c" \
    -o "$build_dir/shm-host"
"$build_dir/shm-host"

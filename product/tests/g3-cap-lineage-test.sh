#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g3"
mkdir -p "$build_dir"
binary="$build_dir/g3_cap_lineage_host"
log="$build_dir/g3-cap-lineage.log"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/product/kernel/include" \
    "$repo_root/product/tests/g3_cap_lineage_host.c" \
    "$repo_root/product/kernel/src/capability_table.c" \
    -o "$binary"

"$binary" 2>&1 | tee "$log"
echo 'PASS: G3 capability lineage revoke and bounded derivation contracts'

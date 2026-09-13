#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g3"
mkdir -p "$build_dir"
gcc -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/product/kernel/include" \
    "$repo_root/product/tests/g3_dynamic_endpoint_host.c" \
    "$repo_root/product/kernel/src/ipc_endpoint.c" \
    -o "$build_dir/g3-dynamic-endpoint-host"
"$build_dir/g3-dynamic-endpoint-host" | tee "$build_dir/g3-dynamic-endpoint-host.log"
echo 'PASS: bounded endpoint object lifecycle rejects closed and destroyed use'

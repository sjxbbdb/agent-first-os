#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" \
  "$repo_root/product/tests/g3_dynamic_endpoint_red.c" "$repo_root/product/kernel/src/ipc_endpoint.c" -o "$repo_root/build/g3-dynamic-endpoint-red"

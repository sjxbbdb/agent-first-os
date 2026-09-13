#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$repo_root/build/g4"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" \
 "$repo_root/product/kernel/src/process.c" "$repo_root/product/kernel/src/capability_table.c" \
 "$repo_root/product/tests/g4-clean-restart-cap-host.c" -o "$repo_root/build/g4/clean-restart-cap-host"
"$repo_root/build/g4/clean-restart-cap-host"

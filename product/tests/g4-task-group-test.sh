#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g4"
mkdir -p "$build_dir"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" \
    "$repo_root/product/kernel/src/process.c" \
    "$repo_root/product/kernel/src/capability_table.c" \
    "$repo_root/product/tests/g4_task_group_host.c" -o "$build_dir/task-group-host"
"$build_dir/task-group-host"

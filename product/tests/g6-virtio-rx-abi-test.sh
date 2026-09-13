#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include" -I"$repo_root/product/userland/include" "$repo_root/product/tests/g6-virtio-rx-abi-test.c" -o "$repo_root/build/g4/g6-rx-abi"
"$repo_root/build/g4/g6-rx-abi"

#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/g2"
mkdir -p "$build_dir"
cc_flags=(-std=c11 -Wall -Wextra -Werror -I"$repo_root/product/kernel/include")
gcc "${cc_flags[@]}" "$repo_root/product/kernel/src/elf_user_loader.c" "$repo_root/product/tests/g2_elf_loader_host.c" -o "$build_dir/elf-loader-host"
"$build_dir/elf-loader-host"
gcc "${cc_flags[@]}" "$repo_root/product/kernel/src/initrd.c" "$repo_root/product/tests/g2_initrd_dynamic_host.c" -o "$build_dir/initrd-dynamic-host"
"$build_dir/initrd-dynamic-host"
if gcc -fsanitize=address,undefined "${cc_flags[@]}" "$repo_root/product/kernel/src/elf_user_loader.c" "$repo_root/product/tests/g2_elf_loader_host.c" -o "$build_dir/elf-loader-host-sanitize" 2>/dev/null; then
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$build_dir/elf-loader-host-sanitize"
else
    echo "ELF loader sanitizer build unavailable; regular host check passed"
fi

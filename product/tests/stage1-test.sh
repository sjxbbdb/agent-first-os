#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"

bash "$repo_root/product/tools/build-bios.sh"
bash "$repo_root/product/tools/run-bios.sh"

cp "$build_dir/stage1.img" "$build_dir/stage1-bad-signature.img"
printf '\x00\x00' | dd of="$build_dir/stage1-bad-signature.img" bs=1 seek=510 conv=notrunc status=none

if bash "$repo_root/product/tools/run-bios.sh" "$build_dir/stage1-bad-signature.img"; then
    echo 'bad-signature image unexpectedly reached Stage 1' >&2
    exit 1
fi

if grep -Fq 'S1 BIOS OK - stage2 pending' "$build_dir/qemu-stage1-bad-signature.log"; then
    echo 'bad-signature image printed Stage 1 marker' >&2
    exit 1
fi

echo 'PASS: valid Stage 1 boots; bad signature does not reach Stage 1'

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp_dir="${TMPDIR:-/tmp}/agent-os-g5-g6-contract"
rm -rf "$tmp_dir"
mkdir -p "$tmp_dir"

cat > "$tmp_dir/header-check.c" <<'EOF'
#include "virtio_protocol.h"
#include "service_protocol.h"
int main(void) {
    return sizeof(AgentOsVirtioInputEvent) == 16 &&
           sizeof(AgentOsServiceRequest) == 56 &&
           sizeof(AgentOsServiceResponse) == 48 &&
           sizeof(AgentOsFileRange) == 16 ? 0 : 1;
}
EOF
gcc -std=c11 -Wall -Wextra -Werror -I"$repo_root/product/services" \
    -I"$repo_root/product/userland/include" \
    -I"$repo_root/product/kernel/include" \
    "$tmp_dir/header-check.c" -o "$tmp_dir/header-check"
"$tmp_dir/header-check"

python3 - "$repo_root/product/kernel/arch/x86_64/boot/uefi/README.md" \
    "$repo_root/product/services/virtio-protocol.md" <<'PY'
import sys
for path in sys.argv[1:]:
    text = open(path, encoding="utf-8").read()
    if "当前" not in text or "未实现" not in text:
        raise SystemExit(f"missing explicit implementation boundary: {path}")
print("g5/g6 contract headers and implementation boundaries: PASS")
PY

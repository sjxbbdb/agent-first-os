# 复跑前 source 本文件
export LP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$LP/tools/env/bin:$LP/tools/qemu-root/usr/bin:$PATH"
# qemu 的库只给 qemu 用，避免污染 gcc：通过 run-qemu.sh 里的 QEMU_LD 传
export QEMU_LD="$LP/tools/qemu-root/usr/lib/x86_64-linux-gnu"
export QEMU_DATA="$LP/tools/qemu-root/usr/share/qemu"
export KV=6.18.60 BB=1.36.1
# 内核 host 工具（objtool 需要 libelf，来自 conda env）
export TENV="$LP/tools/env"

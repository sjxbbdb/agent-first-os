#!/bin/bash
# 安装构建工具到 tools/（micromamba + conda-forge，无需 root/apt）
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); cd "$P/tools"
[ -x bin/micromamba ] || { curl -Ls -o micromamba.tar.bz2 https://micro.mamba.pm/api/micromamba/linux-64/latest; python3 -c "import tarfile;tarfile.open(\"micromamba.tar.bz2\").extract(\"bin/micromamba\")"; }
export MAMBA_ROOT_PREFIX="$P/tools/mamba"
./bin/micromamba create -y -p "$P/tools/env" -c conda-forge make flex bison bc cpio m4 bzip2 elfutils openssl pkg-config zlib
./bin/micromamba list -p "$P/tools/env"

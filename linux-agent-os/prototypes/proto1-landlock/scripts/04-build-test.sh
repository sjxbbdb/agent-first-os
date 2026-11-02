#!/bin/bash
# 静态编译测试程序，头文件用内核源码树 make headers 导出的 usr/include
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); source "$P/env.sh"
KH="$P/build/linux-$KV/usr/include"; test -f "$KH/linux/landlock.h"
mkdir -p "$P/build/out"; gcc --version | head -1
gcc -static -O2 -Wall -Wextra -isystem "$KH" -o "$P/build/out/landlock_test" "$P/src/landlock_test.c"
readelf -lW "$P/build/out/landlock_test" | grep -E "Elf file type|INTERP" || true
sha256sum "$P/build/out/landlock_test" | tee "$P/artifacts/landlock_test.sha256"

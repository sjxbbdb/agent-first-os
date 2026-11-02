#!/bin/bash
# 从零全流程（工具 -> 下载校验 -> 内核 -> busybox -> 测试程序 -> initramfs -> QEMU T1 + T2 两次）
set -euo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); cd "$P"
bash scripts/00-install-tools.sh   > logs/tools-install.log 2>&1
bash scripts/00b-install-qemu-debs.sh > logs/tools-install-qemu.log 2>&1
bash scripts/01-download.sh        > logs/download-verify.log 2>&1
bash scripts/02-build-kernel.sh    > logs/kernel-build.log 2>&1
bash scripts/03-build-busybox.sh   > logs/busybox-build.log 2>&1
bash scripts/04-build-test.sh      > logs/test-build.log 2>&1
for t in t1 t2; do bash scripts/05-build-initramfs.sh $t > logs/initramfs-build-$t.log 2>&1; done
bash scripts/run-qemu.sh t1 qemu-t1-serial.log
bash scripts/run-qemu.sh t2 qemu-t2-serial-run1.log
bash scripts/run-qemu.sh t2 qemu-t2-serial-run2.log

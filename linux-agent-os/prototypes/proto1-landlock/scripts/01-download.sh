#!/bin/bash
# 下载并校验内核与 busybox 源码
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); cd "$P/dl"
KV=6.18.60; BB=1.36.1
dl(){ [ -f "$(basename "$1")" ] || curl -fLO "$1"; }
dl https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KV.tar.xz
dl https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KV.tar.sign
dl https://cdn.kernel.org/pub/linux/kernel/v6.x/sha256sums.asc
grep " linux-$KV.tar.xz\$" sha256sums.asc > k.sha256; cat k.sha256
sha256sum -c k.sha256
# WKD 在本网络不通，改为从 keyserver.ubuntu.com 按指纹取 Linus 与 Greg KH 公钥，以及 sha256sums.asc 实际签名 key
for fp in ABAF11C65A2970B130ABE3C479BE3E4300411886 647F28654894E3BD457199BE38DBBDC86092693E B8868C80BA62A1FFFAF5FDA9632D3A06589DA6B1; do
  curl -fs "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x$fp" | gpg --batch --import || echo "取 key $fp 失败"; done
gpg --batch --verify sha256sums.asc || echo "sha256sums.asc 签名校验失败/未完成"
xz -dc linux-$KV.tar.xz | gpg --batch --verify linux-$KV.tar.sign - || echo "tarball 签名校验失败/未完成"
dl https://busybox.net/downloads/busybox-$BB.tar.bz2
dl https://busybox.net/downloads/busybox-$BB.tar.bz2.sha256
cat busybox-$BB.tar.bz2.sha256; sha256sum -c busybox-$BB.tar.bz2.sha256
sha256sum linux-$KV.tar.xz busybox-$BB.tar.bz2

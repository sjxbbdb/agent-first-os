#!/bin/bash
# 用法: 05-build-initramfs.sh t1|t2  -> build/out/initramfs-<t>.cpio.gz
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); source "$P/env.sh"; T=${1:-t2}
R="$P/build/rootfs-$T"; rm -rf "$R"; mkdir -p "$R"/{bin,sbin,usr/bin,usr/sbin,proc,sys,dev,etc,root,tmp/work}
cp "$P/build/busybox-$BB/busybox" "$R/bin/busybox"
for a in $("$R/bin/busybox" --list-full); do [ -e "$R/$a" ] || ln -s /bin/busybox "$R/$a"; done
cp "$P/initramfs-src/init-$T" "$R/init"; chmod 755 "$R/init"
echo "top-secret-for-C3" > "$R/root/secret"; chmod 600 "$R/root/secret"
[ "$T" = t2 ] && install -m 755 "$P/build/out/landlock_test" "$R/usr/bin/landlock_test"
mkdir -p "$P/build/out"
# 非 root 无法 mknod /dev/console，因此用内核自带 usr/gen_init_cpio.c（只编译、不改）按清单打包：
# 清单里所有条目归 0:0，并加 nod /dev/console（内核在执行 /init 前就要打开它）
gcc -O2 -o "$P/build/out/gen_init_cpio" "$P/build/linux-$KV/usr/gen_init_cpio.c"
L="$P/build/out/initramfs-$T.list"
python3 - "$R" > "$L" <<'PY2'
import os,sys,stat
r=sys.argv[1]
print("nod /dev/console 0600 0 0 c 5 1")
for d,ds,fs in sorted(os.walk(r)):
    ds.sort()
    for n in sorted(ds+fs):
        a=os.path.join(d,n); rel="/"+os.path.relpath(a,r); st=os.lstat(a); m=oct(stat.S_IMODE(st.st_mode))[2:]
        if stat.S_ISLNK(st.st_mode): print(f"slink {rel} {os.readlink(a)} 777 0 0")
        elif stat.S_ISDIR(st.st_mode): print(f"dir {rel} {m} 0 0")
        else: print(f"file {rel} {a} {m} 0 0")
PY2
# /dev 目录必须先于 nod 出现：把 nod 行移到末尾
{ grep -v '^nod ' "$L"; echo "nod /dev/console 0600 0 0 c 5 1"; } > "$L.tmp" && mv "$L.tmp" "$L"
"$P/build/out/gen_init_cpio" -t 0 "$L" | gzip -9 -n > "$P/build/out/initramfs-$T.cpio.gz"
ls -l "$P/build/out/initramfs-$T.cpio.gz"; sha256sum "$P/build/out/initramfs-$T.cpio.gz" | tee "$P/artifacts/initramfs-$T.sha256"

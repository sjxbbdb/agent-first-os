#!/bin/bash
# conda-forge 没有 qemu：直接从 deb.debian.org 拉 trixie 的 .deb，dpkg -x 解到 tools/qemu-root（不安装、不需 root）
set -euxo pipefail
P=$(cd "$(dirname "$0")/.." && pwd); D="$P/tools/debs"; R="$P/tools/qemu-root"; mkdir -p "$D" "$R"; cd "$D"
[ -f Packages.xz ] || curl -fsSO http://deb.debian.org/debian/dists/trixie/main/binary-amd64/Packages.xz
python3 - <<'PY' > urls.txt
import lzma,re,subprocess
pk={};cur={}
for line in lzma.open("Packages.xz","rt"):
    line=line.rstrip("\n")
    if not line:
        if cur: pk.setdefault(cur["Package"],cur)
        cur={};continue
    if line[0]!=" " and ":" in line:
        k,v=line.split(":",1);cur[k]=v.strip()
    prov={}
inst=set(subprocess.run(["dpkg-query","-W","-f=${Package}\n"],capture_output=True,text=True).stdout.split())
for p,c in pk.items():
    for x in c.get("Provides","").split(","):
        x=x.strip().split(" ")[0]
        if x: prov.setdefault(x,p)
need=[];seen=set();todo=["qemu-system-x86"]
while todo:
    n=todo.pop()
    if n in seen: continue
    seen.add(n)
    if n in inst: continue
    if n not in pk:
        if n in prov: n=prov[n]
        else: continue
    if n in inst: continue
    need.append(n)
    for alt in (pk[n].get("Depends","")+","+pk[n].get("Pre-Depends","")).split(","):
        alt=alt.strip()
        if not alt: continue
        names=[re.split(r"[ (:]",a.strip())[0] for a in alt.split("|")]
        if any(x in inst for x in names): continue
        todo.append(names[0])
for n in need: print("http://deb.debian.org/debian/"+pk[n]["Filename"])
PY
cat urls.txt
while read u; do f=$(basename "$u"); [ -f "$f" ] || curl -fsSLO "$u"; dpkg -x "$f" "$R"; done < urls.txt
sha256sum *.deb

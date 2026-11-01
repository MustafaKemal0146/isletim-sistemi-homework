#!/usr/bin/env bash
# olcum.sh - repeatable measurements: N runs per scenario, mean +- stddev.
#
# Output (OLCUM_DIZIN, default olcum/):
#   <TARIH>-olcum.csv  tekrar,senaryo,syscall_sayisi,engellenen,sure_ms,rss_kb,exit_kodu
#   <TARIH>-ozet.md    environment + mean/stddev/min/max table
#   <TARIH>-sure.png   (if python3-matplotlib is present)
# Env: BIN, PROFIL, TEKRAR (5), TARIH, OLCUM_DIZIN, ISINMA (1 discarded warm-up run)
#
# Definitions (see docs/05-olcum-yontemi.md):
#   sure_ms, rss_kb  : wall time / peak RSS of the target tree, from sandbox --olcum (wait4 rusage)
#   engellenen       : say -> blocked calls counted by ptrace; kill -> 1 if SIGSYS else 0; yok -> 0
#   syscall_sayisi   : syscalls the target makes, counted from an unsandboxed `strace -f` trace
#   exit_kodu        : 128+N when killed by signal N
set -euo pipefail
export LC_ALL=C   # awk/strace decimal point must not follow the user locale
cd "$(dirname "$0")/.."
BIN="${BIN:-bin}"
PROFIL="${PROFIL:-profiller/hedef.seccomp}"
TEKRAR="${TEKRAR:-5}"
ISINMA="${ISINMA:-1}"
TARIH="${TARIH:-$(date +%F)}"
OLCUM_DIZIN="${OLCUM_DIZIN:-olcum}"
mkdir -p "$OLCUM_DIZIN"
CSV="$OLCUM_DIZIN/$TARIH-olcum.csv"
OZET="$OLCUM_DIZIN/$TARIH-ozet.md"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

[ -x "$BIN/sandbox" ] || { echo "olcum: $BIN/sandbox yok (make)" >&2; exit 3; }
[ -f "$PROFIL" ] || { echo "olcum: profil yok: $PROFIL (scripts/calistir.sh)" >&2; exit 5; }

# senaryo adi : sandbox modu : hedef argumani
SENARYOLAR=(
    "yok-tam:yok:tam"
    "kill-tam:kill:tam"
    "errno-tam:errno:tam"
    "say-tam:say:tam"
    "say-ihlal:say:ihlal"
    "kill-ihlal:kill:ihlal"
)

# Unsandboxed syscall count of the target for one scenario argument.
cagri_say() {
    local f="$tmp/iz.txt"
    strace -f -qq -s 0 -o "$f" -- "$BIN/hedef" "$1" >/dev/null 2>&1 || true
    awk '$2 ~ /^[a-z_0-9]+\(/ {n++} END{print n+0}' "$f"
}

kos() { # $1 mod, $2 hedef arg -> prints the SANDBOX_OLCUM line
    local mod="$1" arg="$2" out
    if [ "$mod" = yok ]; then
        out="$("$BIN/sandbox" --mod yok --olcum -- "$BIN/hedef" "$arg" 2>&1 >/dev/null || true)"
    else
        out="$("$BIN/sandbox" --mod "$mod" --profil "$PROFIL" --olcum -- "$BIN/hedef" "$arg" 2>&1 >/dev/null || true)"
    fi
    grep '^SANDBOX_OLCUM' <<<"$out" || { echo "olcum: sandbox olcum satiri yok ($mod $arg)" >&2; echo "$out" >&2; return 1; }
}

alan() { sed -n "s/.*\b$2=\([^ ]*\).*/\1/p" <<<"$1"; }

if [ -z "${SADECE_OZET:-}" ]; then   # SADECE_OZET=1: rebuild summary/PNG from an existing CSV
echo "tekrar,senaryo,syscall_sayisi,engellenen,sure_ms,rss_kb,exit_kodu" >"$CSV"
for sen in "${SENARYOLAR[@]}"; do
    IFS=: read -r ad mod arg <<<"$sen"
    for ((w = 0; w < ISINMA; w++)); do kos "$mod" "$arg" >/dev/null; done
    for ((r = 1; r <= TEKRAR; r++)); do
        satir="$(kos "$mod" "$arg")"
        n="$(cagri_say "$arg")"
        echo "$r,$ad,$n,$(alan "$satir" engellenen),$(alan "$satir" sure_ms),$(alan "$satir" rss_kb),$(alan "$satir" exit)" >>"$CSV"
    done
    echo "olcum: $ad tamam ($TEKRAR tekrar)" >&2
done

fi
# ---- summary ----
KAPAT="$("$BIN/profil_uret" "${PROFIL%.seccomp}.json" -o "$tmp/x.seccomp" --ozet 2>/dev/null || echo "(profil JSON yok)")"
{
    echo "# Olcum ozeti — $TARIH"
    echo
    echo "Komut: \`TEKRAR=$TEKRAR tools/olcum.sh\` — her senaryoda $ISINMA isinma kosusu atildi, $TEKRAR olculdu."
    echo
    echo "## Ortam"
    echo '```'
    uname -a
    gcc --version | head -1
    echo "OS: $(. /etc/os-release; echo "$PRETTY_NAME") ; $(ldd --version | head -1)"
    echo "yama.ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null) dmesg_restrict=$(cat /proc/sys/kernel/dmesg_restrict)"
    if v=$(pkg-config --modversion libseccomp 2>/dev/null); then echo "libseccomp: $v (sistem)"
    else echo "libseccomp: $(sed -n 's/^Version: //p' .deps/usr/lib/x86_64-linux-gnu/pkgconfig/libseccomp.pc) (.deps, statik)"; fi
    strace -V | head -1
    grep -m1 'model name' /proc/cpuinfo || true
    echo "nproc: $(nproc)"
    echo "profil: $PROFIL sha256=$(sha256sum "$PROFIL" | cut -c1-16)"
    echo '```'
    echo
    echo "## Sonuclar (ortalama ± standart sapma, n=$TEKRAR)"
    echo
    echo "| Senaryo | syscall sayisi (ort) | engellenen (ort) | sure_ms | min–max ms | rss_kb | exit |"
    echo "|---|---|---|---|---|---|---|"
    awk -F, 'NR>1 {
        s=$2; n[s]++; scs[s]+=$3; engs[s]+=$4; ex[s]=$7;
        t=$5; sum[s]+=t; sq[s]+=t*t; if(!(s in mn)||t<mn[s])mn[s]=t; if(!(s in mx)||t>mx[s])mx[s]=t;
        r=$6; rs[s]+=r; rq[s]+=r*r;
        if(!(s in ord)){ord[s]=++k; nm[k]=s}
    } END {
        for(i=1;i<=k;i++){ s=nm[i]; m=sum[s]/n[s]; v=(n[s]>1)?(sq[s]-n[s]*m*m)/(n[s]-1):0; if(v<0)v=0;
          rm=rs[s]/n[s]; rv=(n[s]>1)?(rq[s]-n[s]*rm*rm)/(n[s]-1):0; if(rv<0)rv=0;
          printf "| %s | %.1f | %.1f | %.2f ± %.2f | %.2f–%.2f | %.0f ± %.0f | %d |\n", s, scs[s]/n[s], engs[s]/n[s], m, sqrt(v), mn[s], mx[s], rm, sqrt(rv), ex[s] }
    }' "$CSV"
    echo
    echo "syscall sayisi ve engellenen: tekrarlarin ortalamasi; exit: tum tekrarlarda ayni deger. Standart sapma: ornek sapmasi (n-1). Ham veri: \`$CSV\`."
    echo
    echo "## Kapatilan system call sayisi"
    echo '```'
    echo "$KAPAT"
    echo '```'
} >"$OZET"

if python3 -c 'import matplotlib' 2>/dev/null; then
    python3 tools/grafik.py "$CSV" "$OLCUM_DIZIN/$TARIH-sure.png" || echo "olcum: grafik uretilemedi (kritik degil)" >&2
fi
echo "olcum: $CSV, $OZET" >&2

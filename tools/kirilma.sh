#!/usr/bin/env bash
# kirilma.sh - break tests (F5). Each subcommand prints a commented transcript; the wrapper
# targets in the report tee them into olcum/kirilma-<ad>.txt.
#   kirilma.sh kill9 | disk | bellek | cpu
# No root needed. disk uses a 20 MB tmpfs in a user namespace (the real disk is never filled);
# bellek/cpu use `systemd-run --user --scope` (cgroup v2, user delegation).
set -uo pipefail
export LC_ALL=C
cd "$(dirname "$0")/.."
ROOT="$PWD"
BIN="${BIN:-$ROOT/bin}"
P="${PROFIL:-$ROOT/profiller/hedef.seccomp}"
S="$BIN/sandbox"
tmp="$(mktemp -d)"
trap 'pkill -KILL -x -u "$(id -u)" hedef 2>/dev/null; rm -rf "$tmp"' EXIT

baslik() { echo; echo "### $*"; }
calistir() { echo "\$ $*"; "$@"; }

zombi_var() { ps -u "$(id -u)" -o stat,pid,ppid,comm | awk '$1 ~ /^Z/ && ($4=="hedef" || $4=="sandbox")'; }

kill9() {
    echo "# kirilma-kill9 — $(date '+%F %T')  (komutlar ve ciktilar)"
    baslik "A) calisan hedefe (cocuk surec) kill -9"
    "$S" --mod kill --profil "$P" -- "$BIN/hedef" uzun 30 >"$tmp/out" 2>"$tmp/err" &
    SP=$!
    sleep 1.5
    CP="$(pgrep -P "$SP")"
    echo "\$ sandbox ... -- hedef uzun 30 &      # sandbox pid=$SP, hedef pid=$CP"
    ps -o pid,ppid,stat,comm -p "$SP,$CP"
    echo "\$ kill -9 $CP"
    kill -9 "$CP"
    wait "$SP"; RC=$?
    echo "sandbox cikis kodu: $RC   (beklenen 137 = 128+SIGKILL)"
    echo "--- sandbox stderr:"; cat "$tmp/err"
    echo "--- hedef stdout (olmeden once ne yazdi):"; cat "$tmp/out"
    echo "zombi surec sayisi : $(zombi_var | wc -l)"
    echo "kalan hedef sureci : $(pgrep -cx -u "$(id -u)" hedef)"
    baslik "A') temizlik sonrasi tekrar calisiyor mu?"
    calistir "$S" --mod kill --profil "$P" -- "$BIN/hedef" tam >/dev/null 2>&1; echo "cikis kodu: $? (beklenen 0)"

    baslik "B) sandbox surecinin kendisine kill -9 (cocuk yetim kalir mi?)"
    "$S" --mod kill --profil "$P" -- "$BIN/hedef" uzun 30 >"$tmp/out" 2>"$tmp/err" &
    SP=$!
    sleep 1.5
    CP="$(pgrep -P "$SP")"
    echo "\$ sandbox ... -- hedef uzun 30 &      # sandbox pid=$SP, hedef pid=$CP"
    echo "\$ kill -9 $SP"
    kill -9 "$SP"
    wait "$SP" 2>/dev/null; echo "sandbox cikis kodu: $? (137)"
    sleep 1
    echo "1 sn sonra hedef (pid $CP) durumu:"
    if ps -o pid,ppid,stat,comm -p "$CP" 2>/dev/null | tail -n +2 | grep -q .; then
        ps -o pid,ppid,stat,comm -p "$CP"
        echo "SONUC: cocuk YETIM kaldi ve calismaya devam ediyor"
        kill -9 "$CP" 2>/dev/null
    else
        echo "SONUC: cocuk de sonlandi (yetim yok)"
    fi
    echo "kalan hedef sureci : $(pgrep -cx -u "$(id -u)" hedef)"

    baslik "C) SAY modu: izleyici (sandbox) olurse tracee ne olur (PTRACE_O_EXITKILL)"
    "$S" --mod say --profil "$P" -- "$BIN/hedef" uzun 30 >/dev/null 2>&1 &
    SP=$!
    sleep 1.5
    CP="$(pgrep -P "$SP")"
    kill -9 "$SP"; wait "$SP" 2>/dev/null
    sleep 1
    echo "say modunda kalan hedef sureci: $(pgrep -cx -u "$(id -u)" hedef)  (0 beklenir)"
}

disk() {
    echo "# kirilma-disk — $(date '+%F %T')"
    echo "Gercek disk doldurulmaz: 20 MB tmpfs, user+mount namespace icinde (unshare -Urm)."
    cat >"$tmp/ic.sh" <<IC
set -u
mkdir -p "$tmp/mnt" && mount -t tmpfs -o size=20m tmpfs "$tmp/mnt" || exit 99
echo "\\\$ df -h $tmp/mnt  (onceki)"; df -h "$tmp/mnt" | tail -1
echo "\\\$ sandbox --mod kill --profil hedef.seccomp -- hedef disk $tmp/mnt"
"$S" --mod kill --profil "$P" -- "$BIN/hedef" disk "$tmp/mnt" 2>"$tmp/e"; rc=\$?
cat "$tmp/e"
echo "cikis kodu: \$rc   (3 = CIKIS_SISTEM: tanimli hata, sinyal yok)"
echo "dolgu.bin kaldi mi: \$(ls "$tmp/mnt" | wc -l) dosya"
echo "\\\$ df -h $tmp/mnt  (sonraki)"; df -h "$tmp/mnt" | tail -1
echo
echo "--- ayni tmpfs'i elle doldurup tekrar deneme (dolu disk uzerinde acilis davranisi)"
dd if=/dev/zero of="$tmp/mnt/elle.bin" bs=1M count=64 2>&1 | tail -1
"$S" --mod kill --profil "$P" -- "$BIN/hedef" disk "$tmp/mnt" 2>&1; echo "cikis kodu: \$?"
rm -f "$tmp/mnt/elle.bin"
echo "--- yer acildiktan sonra tekrar:"
"$S" --mod kill --profil "$P" -- "$BIN/hedef" disk "$tmp/mnt" 2>&1 | tail -2; echo "cikis kodu: \${PIPESTATUS[0]}"
IC
    unshare -Urm bash "$tmp/ic.sh" 2>&1
    echo "unshare cikis: $?"
    echo "gercek diskte etkisi: $(df -h "$ROOT" | tail -1 | awk '{print $4" bos"}') (degismedi)"
}

bellek() {
    echo "# kirilma-bellek — $(date '+%F %T')"
    baslik "A) sinir ALTINDA: MemoryMax=32M, hedef bellek 16 MiB (basarili olmali)"
    systemd-run --user --scope --quiet -p MemoryMax=32M -p MemorySwapMax=0 bash -c "
        '$S' --mod kill --profil '$P' -- '$BIN/hedef' bellek 16 2>&1 | tail -2
        echo \"sandbox cikis: \${PIPESTATUS[0]}\"
        cg=/sys/fs/cgroup\$(cut -d: -f3 /proc/self/cgroup); echo \"memory.max=\$(cat \$cg/memory.max) memory.peak=\$(cat \$cg/memory.peak)\"; grep -E '^oom' \$cg/memory.events"
    baslik "B) sinir USTUNDE: MemoryMax=32M, hedef bellek 64 MiB (OOM kill beklenir)"
    T0="$(date '+%Y-%m-%d %H:%M:%S')"
    systemd-run --user --scope --quiet -p MemoryMax=32M -p MemorySwapMax=0 bash -c "
        '$S' --mod kill --profil '$P' -- '$BIN/hedef' bellek 64 2>&1 | tail -4
        echo \"sandbox cikis: \${PIPESTATUS[0]}   (137 = 128+SIGKILL)\"
        cg=/sys/fs/cgroup\$(cut -d: -f3 /proc/self/cgroup); echo \"memory.max=\$(cat \$cg/memory.max) memory.peak=\$(cat \$cg/memory.peak)\"; grep -E '^(oom|oom_kill)' \$cg/memory.events"
    echo "zombi surec sayisi : $(zombi_var | wc -l); kalan hedef: $(pgrep -cx -u "$(id -u)" hedef)"
    echo "--- cekirdek gunlugu (journalctl -k --since '$T0'):"
    journalctl -k --since "$T0" --no-pager 2>&1 | grep -iE 'oom|Killed process|out of memory' | cut -c1-230 | head -4
    baslik "C) OOM sonrasi tekrar calisiyor"
    "$S" --mod kill --profil "$P" -- "$BIN/hedef" bellek 16 >/dev/null 2>&1; echo "cikis: $? (0)"
}

cpu() {
    N="${TEKRAR:-5}"
    echo "# kirilma-cpu — $(date '+%F %T')  (hedef hesap 300M, sandbox KILL + profil, $N tekrar)"
    olc() { # $1 etiket; rest: command prefix
        local et="$1"; shift
        local sure sonuc
        for ((i = 1; i <= N; i++)); do
            out="$("$@" "$S" --mod kill --profil "$P" --olcum -- "$BIN/hedef" hesap 300 2>&1)"
            sure="$(sed -n 's/.*sure_ms=\([0-9.]*\).*/\1/p' <<<"$out")"
            sonuc="$(sed -n 's/.*sonuc=\([0-9a-f]*\).*/\1/p' <<<"$out")"
            echo "$et,$i,$sure,$sonuc" >>"$tmp/cpu.csv"
        done
    }
    : >"$tmp/cpu.csv"
    olc sinirsiz env
    olc quota20 systemd-run --user --scope --quiet -p CPUQuota=20%
    echo "etiket,tekrar,sure_ms,sonuc"; cat "$tmp/cpu.csv"
    echo
    echo "| Kosul | sure_ms (ort ± sd) | min–max | sonuc (tum tekrarlar) |"
    echo "|---|---|---|---|"
    for et in sinirsiz quota20; do
        awk -F, -v e="$et" '$1==e{n++;s+=$3;q+=$3*$3; if(min==""||$3<min)min=$3; if(max==""||$3>max)max=$3; r[$4]=1}
            END{m=s/n; v=(n>1)?(q-n*m*m)/(n-1):0; if(v<0)v=0; k=""; for(x in r)k=k x" "; printf "| %s | %.1f ± %.1f | %.1f–%.1f | %s(%d farkli) |\n", e, m, sqrt(v), min, max, k, length(r)}' "$tmp/cpu.csv"
    done
    awk -F, '$1=="sinirsiz"{a+=$3;na++} $1=="quota20"{b+=$3;nb++} END{printf "yavaslama: x%.2f (beklenen ~5x: %%20 kota)\n", (b/nb)/(a/na)}' "$tmp/cpu.csv"
    cp "$tmp/cpu.csv" "$ROOT/olcum/kirilma-cpu.csv" 2>/dev/null
}

case "${1:-}" in
    kill9) kill9 ;; disk) disk ;; bellek) bellek ;; cpu) cpu ;;
    *) echo "kullanim: $0 kill9|disk|bellek|cpu" >&2; exit 2 ;;
esac

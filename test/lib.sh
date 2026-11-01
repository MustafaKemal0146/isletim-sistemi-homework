#!/usr/bin/env bash
# lib.sh - tiny test helpers. Source from test_*.sh. Env: BIN (default bin), ASAN=1.
set -u
export LC_ALL=C   # byte-order sort must match the C tools (strcmp)
KOK="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-bin}"
case "$BIN" in /*) ;; *) BIN="$KOK/$BIN" ;; esac
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
GECTI=0

gecti() { GECTI=$((GECTI + 1)); echo "  ok   - $1"; }
kaldi() { echo "  FAIL - $1"; [ $# -gt 1 ] && printf '         %s\n' "${@:2}"; exit 1; }

# esit <test adi> <beklenen> <gercek>
esit() { [ "$2" = "$3" ] && gecti "$1" || kaldi "$1" "beklenen: [$2]" "gercek:   [$3]"; }
# icerir <test adi> <metin> <aranan>
icerir() { grep -qF -- "$3" <<<"$2" && gecti "$1" || kaldi "$1" "aranan: [$3]" "metin:" "$2"; }
yok() { grep -qF -- "$3" <<<"$2" && kaldi "$1" "bulunmamali: [$3]" || gecti "$1"; }
# cikis <test adi> <beklenen kod> <komut...>  (stdout+stderr -> $CIKTI)
cikis() {
    local ad="$1" bek="$2"; shift 2
    CIKTI="$("$@" 2>&1)"; local rc=$?
    [ "$rc" -eq "$bek" ] && gecti "$ad" || kaldi "$ad" "beklenen cikis: $bek, gercek: $rc" "$CIKTI"
}

# Build the real profile with the real pipeline (3 scenarios, merged): $TMP/hedef.{json,seccomp,bpf}
profil_hazirla() {
    [ -f "$TMP/hedef.seccomp" ] && return 0
    for s in normal hata socket; do
        "$BIN/profil" -n 2 -o "$TMP/$s.json" -- "$BIN/hedef" "$s" 2>/dev/null || kaldi "profil_hazirla: $s"
    done
    "$BIN/profil" --birlestir -o "$TMP/hedef.json" "$TMP/normal.json" "$TMP/hata.json" "$TMP/socket.json" 2>/dev/null
    "$BIN/profil_uret" "$TMP/hedef.json" -o "$TMP/hedef.seccomp" --bpf "$TMP/hedef.bpf" 2>/dev/null || kaldi "profil_uret"
}

bitir() { echo "  -> $GECTI kontrol gecti"; }

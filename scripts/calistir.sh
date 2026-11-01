#!/usr/bin/env bash
# calistir.sh - end-to-end: profile (3 scenarios, merged) -> filter -> sandboxed runs.
# No root needed. Env: BIN (default bin), TEKRAR (strace runs per scenario, default 3).
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="${BIN:-bin}"
TEKRAR="${TEKRAR:-3}"
CIKTI="${CIKTI:-profiller}"
mkdir -p "$CIKTI"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "== 1/4 profil cikar: normal / hata / socket (strace -f, x$TEKRAR)"
for s in normal hata socket; do
    "$BIN/profil" -n "$TEKRAR" -o "$tmp/$s.json" -- "$BIN/hedef" "$s"
done
"$BIN/profil" --birlestir -o "$CIKTI/hedef.json" "$tmp/normal.json" "$tmp/hata.json" "$tmp/socket.json"

echo "== 2/4 filtre uret (deterministik)"
"$BIN/profil_uret" "$CIKTI/hedef.json" -o "$CIKTI/hedef.seccomp" --bpf "$CIKTI/hedef.bpf" --ozet
sha256sum "$CIKTI/hedef.seccomp" "$CIKTI/hedef.bpf"

echo "== 3/4 profille calistir (KILL): hedef tam"
"$BIN/sandbox" --mod kill --profil "$CIKTI/hedef.seccomp" -- "$BIN/hedef" tam

echo "== 4/4 ihlal: izinsiz unshare, iki mod"
"$BIN/ihlal_testi" --profil "$CIKTI/hedef.seccomp"

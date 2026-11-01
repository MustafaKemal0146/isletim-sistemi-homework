#!/usr/bin/env bash
# Profile extraction: golden parse, live trace, merge.
source "$(dirname "$0")/lib.sh"

echo "[T1] golden: strace -c tablosu -> beklenen JSON (bayt bayt)"
"$BIN/profil" -o "$TMP/g.json" --strace-girdi "$KOK/test/veri/strace-ornek.txt" 2>/dev/null
esit "tablo ayristirma, golden JSON ile ayni" "$(sha256sum <"$KOK/test/veri/beklenen.json")" "$(sha256sum <"$TMP/g.json")"
icerir "hata sayaci okundu (access hata=1)" "$(cat "$TMP/g.json")" '"ad": "access", "cagri": 1, "hata": 1'

echo "[T2] canli profil: strace -f izi"
"$BIN/profil" -o "$TMP/canli.json" -- "$BIN/hedef" normal 2>/dev/null
J="$(cat "$TMP/canli.json")"
for sc in execve write openat mmap clone3 exit_group exit; do icerir "canli profilde $sc var" "$J" "\"ad\": \"$sc\""; done

echo "[T3] -c tablosu exit_group'u kacirir, iz kacirmaz (bilinen sinir)"
yok "golden tabloda exit_group yok" "$(cat "$KOK/test/veri/beklenen.json")" '"ad": "exit_group"'

echo "[T4] birlestirme: birlesim, alfabetik sira, sayaclar toplanir"
"$BIN/profil" -o "$TMP/h.json" -- "$BIN/hedef" hata 2>/dev/null
"$BIN/profil" --birlestir -o "$TMP/b.json" "$TMP/canli.json" "$TMP/h.json" 2>/dev/null
ADLAR="$(grep -o '"ad": "[a-z0-9_]*"' "$TMP/b.json" | cut -d'"' -f4)"
esit "adlar sirali ve tekrarsiz" "$(sort -u <<<"$ADLAR")" "$ADLAR"
icerir "hata senaryosundan connect geldi" "$(cat "$TMP/b.json")" '"ad": "connect"'
bitir

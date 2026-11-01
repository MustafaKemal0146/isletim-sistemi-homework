#!/usr/bin/env bash
# Profile extraction: golden parse, live trace, merge.
source "$(dirname "$0")/lib.sh"

echo "[T1] golden: strace -c tablosu -> beklenen JSON (bayt bayt)"
"$BIN/profil" -o "$TMP/g.json" --strace-girdi "$KOK/test/veri/strace-ornek.txt" 2>/dev/null
esit "tablo ayristirma, golden JSON ile ayni (libseccomp surum satiri haric)" "$(grep -v '"libseccomp"' "$KOK/test/veri/beklenen.json" | sha256sum)" "$(grep -v '"libseccomp"' "$TMP/g.json" | sha256sum)"
icerir "hata sayaci okundu (access hata=1)" "$(cat "$TMP/g.json")" '"ad": "access", "cagri": 1, "hata": 1'

echo "[T2] canli profil: strace -f izi"
"$BIN/profil" -o "$TMP/canli.json" -- "$BIN/hedef" normal 2>/dev/null
J="$(cat "$TMP/canli.json")"
for sc in execve write openat mmap clone3 exit_group exit; do icerir "canli profilde $sc var" "$J" "\"ad\": \"$sc\""; done

echo "[T3] -c tablosu exit_group'u kacirir, iz kacirmaz (bilinen sinir, GERCEK strace -c kosusuyla)"
LC_ALL=C strace -f -qq -c -o "$TMP/c.txt" -- "$BIN/hedef" normal >/dev/null 2>&1
"$BIN/profil" -o "$TMP/c.json" --strace-girdi "$TMP/c.txt" 2>/dev/null
yok "canli -c tablosunda exit_group yok" "$(cat "$TMP/c.json")" '"ad": "exit_group"'
icerir "ayni kosunun tam izinde exit_group var" "$J" '"ad": "exit_group"'

echo "[T4] birlestirme: birlesim, alfabetik sira, sayaclar toplanir"
"$BIN/profil" -o "$TMP/h.json" -- "$BIN/hedef" hata 2>/dev/null
"$BIN/profil" --birlestir -o "$TMP/b.json" "$TMP/canli.json" "$TMP/h.json" 2>/dev/null
ADLAR="$(grep -o '"ad": "[a-z0-9_]*"' "$TMP/b.json" | cut -d'"' -f4)"
esit "adlar sirali ve tekrarsiz" "$(sort -u <<<"$ADLAR")" "$ADLAR"
printf '{"surum":1,"syscalls":[{"ad":"read","cagri":3,"hata":1},{"ad":"write","cagri":2,"hata":0}]}' >"$TMP/s1.json"
printf '{"surum":1,"syscalls":[{"ad":"read","cagri":4,"hata":2}]}' >"$TMP/s2.json"
"$BIN/profil" --birlestir -o "$TMP/s.json" "$TMP/s1.json" "$TMP/s2.json" 2>/dev/null
icerir "sayaclar toplandi (read 3+4=7, hata 1+2=3)" "$(cat "$TMP/s.json")" '{"ad": "read", "cagri": 7, "hata": 3}'
icerir "toplam_cagri 9" "$(cat "$TMP/s.json")" '"toplam_cagri": 9'
icerir "hata senaryosundan connect geldi" "$(cat "$TMP/b.json")" '"ad": "connect"'
bitir

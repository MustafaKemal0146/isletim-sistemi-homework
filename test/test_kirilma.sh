#!/usr/bin/env bash
# Lifecycle break tests that need no special privileges (full break tests: tools/kirilma.sh).
source "$(dirname "$0")/lib.sh"
profil_hazirla
P="$TMP/hedef.seccomp"

yasiyor() { kill -0 "$1" 2>/dev/null && [ "$(ps -o stat= -p "$1" | cut -c1)" != Z ]; }

echo "[T25] kill -9 hedefe: sandbox 137 raporlar, zombi/yetim yok, tekrar calisir"
"$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" uzun 30 >/dev/null 2>"$TMP/e1" &
SP=$!
sleep 1
CP="$(pgrep -P "$SP")"
[ -n "$CP" ] && gecti "cocuk bulundu" || kaldi "cocuk bulunamadi"
kill -9 "$CP"
wait "$SP"; RC=$?
esit "sandbox cikis 137" "137" "$RC"
icerir "sinyal 9 raporlandi" "$(cat "$TMP/e1")" "9 (Killed)"
yasiyor "$CP" && kaldi "hedef hala yasiyor" || gecti "hedef yok, zombi degil"
cikis "temizlik sonrasi tekrar calisiyor" 0 "$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" tam

echo "[T26] kill -9 sandbox'a: cocuk yetim kalmaz (PR_SET_PDEATHSIG), say modunda da (EXITKILL)"
for mod in kill say; do
    "$BIN/sandbox" --mod $mod --profil "$P" -- "$BIN/hedef" uzun 30 >/dev/null 2>&1 &
    SP=$!
    sleep 1
    CP="$(pgrep -P "$SP")"
    kill -9 "$SP"; wait "$SP" 2>/dev/null
    sleep 0.5
    yasiyor "$CP" && { kill -9 "$CP"; kaldi "$mod: cocuk yetim kaldi"; } || gecti "$mod: cocuk de sonlandi"
done

echo "[T27] hesap senaryosu: sonuc profilli/profilsiz ayni (CPU kotasi testinin dogruluk olcutu)"
A="$("$BIN/hedef" hesap 20)"
B="$("$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" hesap 20 2>/dev/null)"
esit "hesap ciktisi ayni" "$A" "$B"
bitir

#!/usr/bin/env bash
# Sandbox behaviour: allowed run, SIGSYS, EPERM, log, say, strace evidence.
source "$(dirname "$0")/lib.sh"
profil_hazirla
P="$TMP/hedef.seccomp"

echo "[T10] izinli calisma: profille cikti profilsizle ayni"
BAS="$("$BIN/hedef" tam 2>/dev/null)"
cikis "KILL modunda hedef tam basarili" 0 "$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" tam
SON="$("$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" tam 2>/dev/null)"
esit "stdout profilsiz ile ayni" "$BAS" "$SON"

echo "[T11] izinsiz syscall + KILL -> SIGSYS (31), cikis 159"
cikis "KILL: cikis kodu 128+31" 159 "$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" ihlal
icerir "rapor SIGSYS diyor" "$CIKTI" "31 (Bad system call)"
yok "ihlal satiri hic yazilmadi (olum cagri aninda)" "$CIKTI" "hedef: ihlal unshare"

echo "[T12] izinsiz syscall + ERRNO -> EPERM, surec yasar"
cikis "ERRNO: cikis 0" 0 "$BIN/sandbox" --mod errno --profil "$P" -- "$BIN/hedef" ihlal
icerir "unshare ret=-1 errno=1" "$CIKTI" "ret=-1 errno=1 (Operation not permitted)"

echo "[T13] LOG modu: cagri calisir (kernel audit'e yazar)"
cikis "LOG: cikis 0" 0 "$BIN/sandbox" --mod log --profil "$P" -- "$BIN/hedef" ihlal
icerir "unshare gercekten calisti" "$CIKTI" "ret=0 errno=0 (izinli calisti)"

echo "[T14] SAY modu: engellenen sayisi ve adi"
cikis "SAY: cikis 0" 0 "$BIN/sandbox" --mod say --profil "$P" -- "$BIN/hedef" ihlal
icerir "engellenen sayisi 1" "$CIKTI" "engellenen system call sayisi: 1"
icerir "ad unshare" "$CIKTI" "engellendi: unshare"
cikis "SAY izinli senaryoda 0 engel" 0 "$BIN/sandbox" --mod say --profil "$P" -- "$BIN/hedef" tam
icerir "0 engellendi" "$CIKTI" "engellenen system call sayisi: 0"

echo "[T15] strace kaniti: killed by SIGSYS (dmesg gerekmez)"
strace -f -o "$TMP/st.txt" "$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" ihlal >/dev/null 2>&1
icerir "strace: killed by SIGSYS" "$(cat "$TMP/st.txt")" "+++ killed by SIGSYS +++"
icerir "strace: son cagri unshare" "$(grep -B2 'killed by SIGSYS' "$TMP/st.txt")" "unshare"

echo "[T16] ihlal_testi: iki mod birden"
cikis "ihlal_testi BASARILI" 0 "$BIN/ihlal_testi" --profil "$P"
icerir "SIGSYS ve EPERM gosterildi" "$CIKTI" "genel: BASARILI"

echo "[T17] sira: NO_NEW_PRIVS=1 ve Seccomp=2 filtre yuklendikten sonra (root'suz)"
cikis "LOG modunda /proc/self/status okunur" 0 "$BIN/sandbox" --mod log --profil "$P" -- grep -E '^(NoNewPrivs|Seccomp):' /proc/self/status
icerir "NoNewPrivs: 1" "$CIKTI" "NoNewPrivs:	1"
icerir "Seccomp: 2 (filter modu)" "$CIKTI" "Seccomp:	2"
bitir

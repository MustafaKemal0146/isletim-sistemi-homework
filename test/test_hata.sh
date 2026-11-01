#!/usr/bin/env bash
# Error paths: corrupt/missing input, bad flags, empty profile, bad command.
source "$(dirname "$0")/lib.sh"
profil_hazirla
P="$TMP/hedef.seccomp"

echo "[T18] bozuk JSON"
printf '{"surum":1,"syscalls":[{"ad":"read","cagri":1,' >"$TMP/kesik.json"
cikis "kesik JSON -> 5" 5 "$BIN/profil_uret" "$TMP/kesik.json" -o "$TMP/k.seccomp"
icerir "mesaj 'bozuk JSON'" "$CIKTI" "bozuk JSON"
printf 'merhaba dunya' >"$TMP/cop.json"
cikis "JSON olmayan dosya -> 5" 5 "$BIN/profil_uret" "$TMP/cop.json" -o "$TMP/k.seccomp"
printf '{"surum":1}' >"$TMP/alansiz.json"
cikis "syscalls alani yok -> 5" 5 "$BIN/profil_uret" "$TMP/alansiz.json" -o "$TMP/k.seccomp"
printf '{"syscalls":[{"ad":"READ; rm","cagri":1}]}' >"$TMP/kotu.json"
cikis "gecersiz syscall adi -> 5" 5 "$BIN/profil_uret" "$TMP/kotu.json" -o "$TMP/k.seccomp"
printf '{"syscalls":[]} fazlalik' >"$TMP/fazla.json"
cikis "belge sonunda fazlalik -> 5" 5 "$BIN/profil_uret" "$TMP/fazla.json" -o "$TMP/k.seccomp"
cikis "birlestir bozuk girdiyi reddeder" 5 "$BIN/profil" --birlestir -o "$TMP/m.json" "$TMP/kesik.json"

echo "[T19] eksik dosya / kullanim"
cikis "olmayan JSON -> 5" 5 "$BIN/profil_uret" "$TMP/yok.json" -o "$TMP/k.seccomp"
cikis "profil_uret argumansiz -> 2" 2 "$BIN/profil_uret"
cikis "sandbox --profil olmadan -> 2" 2 "$BIN/sandbox" --mod kill -- /bin/true
cikis "sandbox gecersiz mod -> 2" 2 "$BIN/sandbox" --mod foo --profil "$P" -- /bin/true
cikis "olmayan .seccomp -> 5" 5 "$BIN/sandbox" --mod kill --profil "$TMP/yok.seccomp" -- /bin/true
printf 'izin read\n' >"$TMP/basliksiz.seccomp"
cikis "basliksiz .seccomp -> 5" 5 "$BIN/sandbox" --mod kill --profil "$TMP/basliksiz.seccomp" -- /bin/true
printf '# seccomp-izin-listesi v1\nizin READ!\n' >"$TMP/kotu.seccomp"
cikis "bozuk satirli .seccomp -> 5" 5 "$BIN/sandbox" --mod kill --profil "$TMP/kotu.seccomp" -- /bin/true

echo "[T20] bos profil: hicbir sey izinli degil"
printf '{"surum":1,"syscalls":[]}' >"$TMP/bos.json"
"$BIN/profil_uret" "$TMP/bos.json" -o "$TMP/bos.seccomp" 2>/dev/null
esit "bos profil 0 izinli" "# izinli: 0" "$(grep '^# izinli' "$TMP/bos.seccomp")"
if [ "${ASAN:-0}" != 1 ]; then
    cikis "bos profil + ERRNO: execve bile engellenir -> 127" 127 "$BIN/sandbox" --mod errno --profil "$TMP/bos.seccomp" -- /bin/true
    icerir "mesaj exec engellendi" "$CIKTI" "komut calistirilamadi"
    cikis "bos profil + KILL: SIGSYS -> 159" 159 "$BIN/sandbox" --mod kill --profil "$TMP/bos.seccomp" -- /bin/true
fi

echo "[T21] komut bulunamadi"
cikis "olmayan komut (profilsiz mod) -> 127" 127 "$BIN/sandbox" --mod yok -- /olmayan/komut
cikis "profil izinli syscall'i ihlal sayilmaz" 2 "$BIN/ihlal_testi" --profil "$P" --syscall write
bitir

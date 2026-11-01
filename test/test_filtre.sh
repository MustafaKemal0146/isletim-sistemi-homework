#!/usr/bin/env bash
# Filter generation: determinism, format, unknown names, empty profile.
source "$(dirname "$0")/lib.sh"
profil_hazirla

echo "[T5] determinizm: ayni girdi -> ayni .seccomp ve .bpf"
"$BIN/profil_uret" "$TMP/hedef.json" -o "$TMP/a.seccomp" --bpf "$TMP/a.bpf" 2>/dev/null
"$BIN/profil_uret" "$TMP/hedef.json" -o "$TMP/b.seccomp" --bpf "$TMP/b.bpf" 2>/dev/null
esit ".seccomp sha256 esit" "$(sha256sum <"$TMP/a.seccomp")" "$(sha256sum <"$TMP/b.seccomp")"
esit ".bpf sha256 esit" "$(sha256sum <"$TMP/a.bpf")" "$(sha256sum <"$TMP/b.bpf")"

echo "[T6] girdi sirasindan bagimsiz"
"$BIN/profil_uret" "$TMP/hata.json" "$TMP/normal.json" "$TMP/socket.json" -o "$TMP/x.seccomp" 2>/dev/null
"$BIN/profil_uret" "$TMP/socket.json" "$TMP/hata.json" "$TMP/normal.json" -o "$TMP/y.seccomp" 2>/dev/null
esit "farkli girdi sirasi ayni cikti" "$(sha256sum <"$TMP/x.seccomp")" "$(sha256sum <"$TMP/y.seccomp")"

echo "[T7] bicim: baslik, izinli sayisi satirlarla tutarli"
esit "ilk satir baslik" "# seccomp-izin-listesi v1" "$(head -1 "$TMP/a.seccomp")"
N="$(grep -c '^izin ' "$TMP/a.seccomp")"
esit "# izinli sayisi = izin satirlari" "# izinli: $N" "$(grep '^# izinli:' "$TMP/a.seccomp")"
esit "izin satirlari sirali" "$(grep '^izin ' "$TMP/a.seccomp" | sort)" "$(grep '^izin ' "$TMP/a.seccomp")"

echo "[T8] bilinmeyen syscall adi reddedilir (sessizce atlanmaz)"
printf '{"surum":1,"syscalls":[{"ad":"read","cagri":1,"hata":0},{"ad":"olmayan_cagri_xyz","cagri":1,"hata":0}]}' >"$TMP/bil.json"
cikis "bilinmeyen ad -> cikis 5" 5 "$BIN/profil_uret" "$TMP/bil.json" -o "$TMP/bil.seccomp"
icerir "hata mesaji adi soyluyor" "$CIKTI" "olmayan_cagri_xyz"

echo "[T9] --ozet: kapatilan syscall sayisi"
OZ="$("$BIN/profil_uret" "$TMP/hedef.json" -o "$TMP/o.seccomp" --ozet 2>/dev/null)"
T="$(sed -n 's/.*toplam_x86_64=\([0-9]*\).*/\1/p' <<<"$OZ")"; I="$(sed -n 's/.*izinli=\([0-9]*\).*/\1/p' <<<"$OZ")"
esit "izinli = .seccomp satir sayisi" "$(grep -c '^izin ' "$TMP/o.seccomp")" "$I"
esit "kapatilan = toplam - izinli" "kapatilan=$((T - I))" "$(sed -n 's/.*\(kapatilan=[0-9]*\).*/\1/p' <<<"$OZ")"
[ "$T" -gt 300 ] && gecti "toplam syscall makul (>300)" || kaldi "toplam syscall: $T"

echo "[T9b] uzun anahtar ve tam sayi olmayan sayac"
printf '{"cok_uzun_anahtar_adi_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx": 1, "syscalls":[{"ad":"read","cagri":1,"hata":0}]}' >"$TMP/uz.json"
cikis "uzun kok anahtari kabul" 0 "$BIN/profil_uret" "$TMP/uz.json" -o "$TMP/uz.seccomp"
printf '{"syscalls":[{"ad":"read","cagri":1e3,"hata":0}]}' >"$TMP/us.json"
cikis "1e3 sayaci reddedilir" 5 "$BIN/profil_uret" "$TMP/us.json" -o "$TMP/uz.seccomp"
printf '{"syscalls":[{"ad":"read","cagri":99999999999999999999,"hata":0}]}' >"$TMP/tas.json"
cikis "tasan sayac reddedilir" 5 "$BIN/profil_uret" "$TMP/tas.json" -o "$TMP/uz.seccomp"
printf '{"syscalls":[{"ad":"read","cagri":1,"hata":0},{"ad":"zzz_kotu","cagri":1,"hata":0}]}' >"$TMP/ls.json"
cikis "bilinmeyen ad sonda -> 5 (sizinti yolu)" 5 "$BIN/profil_uret" "$TMP/ls.json" -o "$TMP/uz.seccomp"
bitir

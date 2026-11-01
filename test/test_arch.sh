#!/usr/bin/env bash
# ABI bypass attempts (x32, int 0x80) and profile header validation (arch/surum/libseccomp).
source "$(dirname "$0")/lib.sh"
profil_hazirla
P="$TMP/hedef.seccomp"

echo "[T23] x32 / i386 ABI ile izinli 'write' cagrisi filtreyi atlatamaz"
if [ "${ASAN:-0}" != 1 ]; then
    cikis "arch_testi BASARILI" 0 "$BIN/arch_testi" --profil "$P"
    icerir "x32 yakalandi (kill)" "$CIKTI" "mod=kill  yol=x32          -> sinyal 31 (Bad system call)    cagri_calisti=hayir handler_calisti=hayir : YAKALANDI"
    icerir "int 0x80 yakalandi (kill)" "$CIKTI" "yol=int80/i386   -> sinyal 31"
    icerir "SIGSYS handler'i calismadi (yakalanamaz olum)" "$CIKTI" "yol=x32+handler  -> sinyal 31 (Bad system call)    cagri_calisti=hayir handler_calisti=hayir"
    yok "hicbir yolda cagri calismadi" "$CIKTI" "cagri_calisti=EVET"
    yok "hicbir yolda atlatma" "$CIKTI" "ATLATILDI"
fi
# the compiled filter itself: foreign-arch action must be KILL_PROCESS (0x80000000), not KILL_THREAD (0)
SON="$(python3 - "$TMP/hedef.bpf" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
c, jt, jf, k = struct.unpack_from('<HBBI', d, 8)          # instruction 1: jeq AUDIT_ARCH_X86_64
hedef = struct.unpack_from('<HBBI', d, (2 + jf) * 8)      # where a wrong architecture lands
print(hex(k), hex(hedef[3]) if hedef[0] == 0x06 else "ret-degil")
PY
)"
esit "BPF: mimari kontrolu x86_64 ve yanlis mimari -> KILL_PROCESS" "0xc000003e 0x80000000" "$SON"

echo "[T24] profil basligi: arch / surum / libseccomp dogrulamasi"
M="$(uname -m)"
V="$(sed -n 's/.*"libseccomp": "\([0-9.]*\)".*/\1/p' "$TMP/normal.json")"
esit "profil libseccomp surumunu yaziyor" "yes" "$([ -n "$V" ] && echo yes)"
base='{"surum":1,"arch":"%s","libseccomp":"%s","syscalls":[{"ad":"read","cagri":1,"hata":0}]}'
printf "$base" "$M" "$V" >"$TMP/m_ok.json"
cikis "uyumlu baslik kabul" 0 "$BIN/profil_uret" "$TMP/m_ok.json" -o "$TMP/m.seccomp"
printf "$base" "aarch64" "$V" >"$TMP/m_arch.json"
cikis "arch uyusmazligi -> 5" 5 "$BIN/profil_uret" "$TMP/m_arch.json" -o "$TMP/m.seccomp"
icerir "mesaj arch uyusmazligi" "$CIKTI" "arch uyusmazligi"
printf "$base" "$M" "1.0.0" >"$TMP/m_lib.json"
cikis "libseccomp major.minor uyusmazligi -> 5" 5 "$BIN/profil_uret" "$TMP/m_lib.json" -o "$TMP/m.seccomp"
icerir "mesaj libseccomp uyusmazligi" "$CIKTI" "libseccomp uyusmazligi"
MM="$(cut -d. -f1,2 <<<"$V")"
printf "$base" "$M" "$MM.0" >"$TMP/m_micro.json"
cikis "yalniz micro farki kabul" 0 "$BIN/profil_uret" "$TMP/m_micro.json" -o "$TMP/m.seccomp"
printf '{"surum":999,"arch":"%s","syscalls":[{"ad":"read","cagri":1,"hata":0}]}' "$M" >"$TMP/m_sur.json"
cikis "surum 999 -> 5" 5 "$BIN/profil_uret" "$TMP/m_sur.json" -o "$TMP/m.seccomp"
icerir "mesaj surum" "$CIKTI" "desteklenmiyor"
printf '{"surum":1,"arch":"%s","libseccomp":"abc","syscalls":[]}' "$M" >"$TMP/m_bozuk.json"
cikis "libseccomp alani bozuk -> 5" 5 "$BIN/profil_uret" "$TMP/m_bozuk.json" -o "$TMP/m.seccomp"
cikis "--uyusmazligi-yoksay ile arch farki gecer" 0 "$BIN/profil_uret" "$TMP/m_arch.json" -o "$TMP/m.seccomp" --uyusmazligi-yoksay
printf '{"syscalls":[{"ad":"read","cagri":1,"hata":0}]}' >"$TMP/m_yok.json"
cikis "arch alani yok: uyari ile kabul" 0 "$BIN/profil_uret" "$TMP/m_yok.json" -o "$TMP/m.seccomp"
icerir "uyari yazildi" "$CIKTI" "UYARI"
bitir

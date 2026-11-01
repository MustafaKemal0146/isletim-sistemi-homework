#!/usr/bin/env bash
# Measurement script: CSV shape and summary.
source "$(dirname "$0")/lib.sh"
profil_hazirla

echo "[T22] olcum.sh: 5 tekrar, CSV biçimi, ozet"
OLCUM_DIZIN="$TMP/olcum" PROFIL="$TMP/hedef.seccomp" TARIH=test BIN="$BIN" TEKRAR=5 \
    bash "$KOK/tools/olcum.sh" >"$TMP/olcum.log" 2>&1 || kaldi "olcum.sh calismadi" "$(cat "$TMP/olcum.log")"
CSV="$TMP/olcum/test-olcum.csv"
esit "baslik satiri" "tekrar,senaryo,syscall_sayisi,engellenen,sure_ms,rss_kb,exit_kodu" "$(head -1 "$CSV")"
for s in yok-tam kill-tam errno-tam say-tam say-ihlal kill-ihlal; do
    esit "$s: 5 tekrar" "5" "$(awk -F, -v s="$s" '$2==s' "$CSV" | wc -l)"
done
esit "tum satirlar 7 sutun" "0" "$(awk -F, 'NF!=7' "$CSV" | wc -l)"
esit "say-ihlal engellenen=1" "1" "$(awk -F, '$2=="say-ihlal"{print $4}' "$CSV" | sort -u)"
esit "kill-ihlal cikis=159" "159" "$(awk -F, '$2=="kill-ihlal"{print $7}' "$CSV" | sort -u)"
esit "yok-tam engellenen=0" "0" "$(awk -F, '$2=="yok-tam"{print $4}' "$CSV" | sort -u)"
icerir "ozet tablosunda ort ± sapma" "$(cat "$TMP/olcum/test-ozet.md")" "±"
OM="$(awk -F, '$2=="yok-tam"{s+=$5;n++} END{printf "%.2f", s/n}' "$CSV")"
icerir "ozetteki yok-tam ortalamasi CSV'den hesaplananla ayni" "$(grep '^| yok-tam' "$TMP/olcum/test-ozet.md")" "| $OM ±"
bitir

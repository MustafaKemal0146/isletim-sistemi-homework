#!/usr/bin/env bash
# rss_dogrula.sh - cross-check the rss_kb column of olcum.sh against two independent methods.
#   A  /usr/bin/time -v on the target alone      (wait4 rusage of the target process)
#   B  sandbox --mod yok --olcum                  (the column olcum.sh records)
#   C  sandbox around /bin/true                   (sandbox's own footprint; fork-inheritance check)
#   D  cgroup v2 memory.peak of a systemd-run --user --scope (charge in bytes, includes page cache)
# Output is printed as-is; nothing is derived by hand.
set -uo pipefail
export LC_ALL=C
cd "$(dirname "$0")/.."
BIN="${BIN:-bin}"
N="${TEKRAR:-5}"

echo "# rss_dogrula $(date +%F) - $N tekrar"
echo "A) /usr/bin/time -v $BIN/hedef tam  [KB]:"
for ((i = 0; i < N; i++)); do /usr/bin/time -v "$BIN/hedef" tam 2>&1 >/dev/null | awk '/Maximum resident/{print $NF}'; done | paste -sd' '
echo "B) sandbox --mod yok --olcum -- hedef tam  [KB]:"
for ((i = 0; i < N; i++)); do "$BIN/sandbox" --mod yok --olcum -- "$BIN/hedef" tam 2>&1 >/dev/null | sed -n 's/.*rss_kb=\([0-9]*\).*/\1/p'; done | paste -sd' '
echo "C) sandbox --mod yok -- /bin/true (sandbox'in kendi ru_maxrss'i) [KB]:"
for ((i = 0; i < 3; i++)); do "$BIN/sandbox" --mod yok --olcum -- /bin/true 2>&1 | sed -n 's/.*rss_kb=\([0-9]*\).*/\1/p'; done | paste -sd' '
echo "D) cgroup memory.peak, systemd-run --user --scope [bayt]:"
if systemd-run --user --scope --quiet true 2>/dev/null; then
    for ((i = 0; i < 3; i++)); do
        systemd-run --user --scope --quiet bash -c "$BIN/hedef tam >/dev/null 2>&1; cat /sys/fs/cgroup\$(cut -d: -f3 /proc/self/cgroup)/memory.peak" 2>&1
    done | paste -sd' '
else
    echo "(systemd-run --user kullanilamiyor: olculmedi)"
fi

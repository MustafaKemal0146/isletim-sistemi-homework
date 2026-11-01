#!/usr/bin/env bash
# audit_kaniti.sh - trigger a seccomp violation and try every way of seeing the kernel audit record.
# Honest about what is unavailable: results are printed, never invented.
# Without root usually only the strace evidence works (kernel.dmesg_restrict=1, journal needs adm group).
set -uo pipefail
cd "$(dirname "$0")/.."
BIN="${BIN:-bin}"
P="${PROFIL:-profiller/hedef.seccomp}"
T0="$(date '+%Y-%m-%d %H:%M:%S')"

echo "== 1) ihlal: KILL modu, exit kodu"
"$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" ihlal; echo "cikis kodu: $? (128+31 = SIGSYS)"
sleep 1

echo; echo "== 2) strace kaniti (root gerekmez)"
strace -f -o /tmp/audit-strace.$$ "$BIN/sandbox" --mod kill --profil "$P" -- "$BIN/hedef" ihlal >/dev/null 2>&1
grep -B1 -E 'SIGSYS' /tmp/audit-strace.$$ | cut -c1-120; rm -f /tmp/audit-strace.$$

echo; echo "== 3) dmesg (kernel.dmesg_restrict=$(cat /proc/sys/kernel/dmesg_restrict))"
if D="$(dmesg 2>&1)"; then grep -iE 'audit|seccomp|SIGSYS' <<<"$D" | tail -3; else echo "dmesg okunamadi: $D"; fi

echo; echo "== 4) journalctl -k --since '$T0'"
J="$(journalctl -k --since "$T0" --no-pager 2>&1)"
if grep -q 'type=1326' <<<"$J"; then grep 'type=1326' <<<"$J" | sed -E 's/ subj=[^ ]+//; s/ (auid|ses|gid)=[^ ]+//g' | tail -2
else echo "type=1326 satiri yok. journalctl ciktisi:"; head -2 <<<"$J"; fi

echo; echo "== 5) ausearch / auditd"
if command -v ausearch >/dev/null; then ausearch -m SECCOMP -ts recent 2>&1 | tail -3; else echo "ausearch kurulu degil"; fi
echo; echo "Not: audit satiri (type=1326 ... syscall=272 ... sig=31) yalnizca audit alt sistemi aciksa ve okuma yetkisi varsa gorunur."

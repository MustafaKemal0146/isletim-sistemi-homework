#!/usr/bin/env bash
# calistir.sh - end-to-end pipeline. Filled in Faz 2 (profile -> filter -> sandbox).
set -euo pipefail
cd "$(dirname "$0")/.."
make all
echo "Faz 0: uçtan uca akış henüz yok (Faz 2'de eklenecek)." >&2
exit 1

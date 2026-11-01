#!/usr/bin/env bash
# test.sh - clean build + full test suite + ASan/UBSan, in one command.
# Used by `docker compose run --rm test` and CI. No root needed.
set -euo pipefail
cd "$(dirname "$0")/.."
make surum
make temiz
make all
make test
make asan
echo "scripts/test.sh: HEPSI YESIL"

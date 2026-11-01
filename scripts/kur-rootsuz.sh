#!/usr/bin/env bash
# kur-rootsuz.sh - fetch libseccomp-dev WITHOUT root into ./.deps (apt-get download + dpkg-deb -x).
# Fallback for machines where `sudo scripts/kur.sh` is not possible. The Makefile
# picks .deps up automatically and links libseccomp statically.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
(cd "$tmp" && apt-get download libseccomp-dev)
mkdir -p .deps
dpkg-deb -x "$tmp"/libseccomp-dev_*.deb .deps
echo "tamam: $(ls .deps/usr/include/seccomp.h)"

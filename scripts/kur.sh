#!/usr/bin/env bash
# kur.sh - install build/runtime dependencies (Ubuntu 24.04 / WSL2).
# ROOT GEREKIR: yalnızca paket kurulumu için. Projenin geri kalanı root istemez.
set -euo pipefail

PAKETLER=(build-essential pkg-config libseccomp-dev seccomp strace python3-matplotlib)

if [[ $EUID -ne 0 ]]; then
    echo "Bu betik paket kurar; şöyle çalıştırın: sudo $0" >&2
    exit 1
fi

apt-get update
apt-get install -y --no-install-recommends "${PAKETLER[@]}"

echo "--- kurulu sürümler ---"
pkg-config --modversion libseccomp
strace -V | head -1
gcc --version | head -1

#!/usr/bin/env bash
# Configure + build EZSpecCam on Linux using the linux-debug or linux-release
# CMake preset. Only the mock camera driver is built — qhyccd/hamamatsu are
# Windows-only and picam auto-skips when the SDK is missing.
#
# Usage: ./build_preset.sh [debug|release]   (default: debug)

set -euo pipefail

CONFIG="${1:-debug}"

# Prefer the system cmake (3.22+) over any broken vendor cmake in PATH.
# On Ubuntu 22.04 apt installs cmake to /usr/bin; Xilinx/Vitis sometimes ships
# a 3.3-era binary earlier in PATH that depends on libidn.so.11.
if [ -x /usr/bin/cmake ]; then
    CMAKE=/usr/bin/cmake
else
    CMAKE=cmake
fi

cd "$(dirname "$0")"

echo "==> Using cmake: $($CMAKE --version | head -1)"
echo "==> Configuring with preset: linux-${CONFIG}"
"$CMAKE" --preset "linux-${CONFIG}"
echo "==> Building with preset: linux-${CONFIG}"
"$CMAKE" --build --preset "linux-${CONFIG}" --parallel
echo "==> Done. Artifacts under build/linux-${CONFIG}/"

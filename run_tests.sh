#!/usr/bin/env bash
# Run the EZSpecCam test suite on Linux. Defaults to linux-debug; pass
# "linux-release" or "linux-debug" as the first argument to override.
#
# Usage: ./run_tests.sh [linux-debug|linux-release]   (default: linux-debug)

set -euo pipefail

CONFIG="${1:-linux-debug}"
BUILD_DIR="$(cd "$(dirname "$0")" && pwd)/build/${CONFIG}"

# Prefer the system cmake/ctest (apt-installed 3.22+) over any broken vendor
# binary earlier in PATH (Xilinx/Vitis 3.3.x depends on libidn.so.11).
if [ -x /usr/bin/ctest ]; then
    CTEST=/usr/bin/ctest
else
    CTEST=ctest
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory not found: $BUILD_DIR" >&2
    echo "       Run build_preset.sh debug first." >&2
    exit 1
fi

cd "$BUILD_DIR"

echo "============================================"
echo "EZSpecCam Test Runner"
echo "============================================"
echo "Build dir: $BUILD_DIR"
echo "Tests:"
"$CTEST" -N

echo
echo "Running all tests..."
echo "============================================"
"$CTEST" --output-on-failure
exit $?

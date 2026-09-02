#!/usr/bin/env bash
# Bundle EZSpecCam for distribution on Linux: copy the executable, the deployed
# mock plugin, and Qt runtime libraries. Mirrors deploy.bat on Windows but uses
# linuxdeployqt when available (recommended) or a manual fallback otherwise.
#
# Output: ./deploy/  (ezspeccam + plugins/drivers/ + Qt libs)
#
# Usage: ./deploy.sh [linux-debug|linux-release]   (default: linux-release)

set -euo pipefail

CONFIG="${1:-linux-release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/${CONFIG}"
DEPLOY_DIR="${SCRIPT_DIR}/deploy"

# Resolve executable path. Both Debug and Release configurations land under
# bin/<Config>/ because the root CMakeLists sets CMAKE_RUNTIME_OUTPUT_DIRECTORY.
case "${CONFIG##linux-}" in
    debug)   APP_BIN_DIR="${BUILD_DIR}/bin/Debug"   ;;
    release) APP_BIN_DIR="${BUILD_DIR}/bin/Release" ;;
    *)
        echo "ERROR: unknown config '${CONFIG}' (expected linux-debug or linux-release)" >&2
        exit 1
        ;;
esac
APP_BIN="${APP_BIN_DIR}/ezspeccam"

# Prefer system binaries over broken vendor ones (Xilinx/Vitis).
if [ -x /usr/bin/cmake ]; then CMAKE=/usr/bin/cmake; else CMAKE=cmake; fi

echo "============================================"
echo "EZSpecCam Linux Deployment"
echo "============================================"

if [ ! -x "$APP_BIN" ]; then
    echo "ERROR: $APP_BIN not found." >&2
    echo "       Run build_preset.sh ${CONFIG##linux-} first." >&2
    exit 1
fi

# Reset deploy dir.
rm -rf "$DEPLOY_DIR"
mkdir -p "${DEPLOY_DIR}/plugins/drivers"

echo "Copying executable..."
cp "$APP_BIN" "$DEPLOY_DIR/"

PLUGIN_SRC="${APP_BIN_DIR}/plugins/drivers"
if [ -d "$PLUGIN_SRC" ]; then
    echo "Copying camera driver plugins from ${PLUGIN_SRC}..."
    PLUGIN_COUNT=0
    for f in "$PLUGIN_SRC"/*.so; do
        [ -e "$f" ] || continue
        echo "  $(basename "$f")"
        cp "$f" "${DEPLOY_DIR}/plugins/drivers/"
        PLUGIN_COUNT=$((PLUGIN_COUNT + 1))
    done
    if [ "$PLUGIN_COUNT" -eq 0 ]; then
        echo "WARNING: no camera driver plugins found under ${PLUGIN_SRC}" >&2
    fi
else
    echo "WARNING: plugin source directory missing: ${PLUGIN_SRC}" >&2
fi

if command -v linuxdeployqt >/dev/null 2>&1; then
    echo "Running linuxdeployqt..."
    QMAKE_FLAG=()
    if [ -x /usr/bin/qmake6 ]; then
        QMAKE_FLAG=(-qmake=/usr/bin/qmake6)
    elif command -v qmake6 >/dev/null 2>&1; then
        QMAKE_FLAG=(-qmake="$(command -v qmake6)")
    else
        echo "WARNING: qmake6 not found; linuxdeployqt may pick the wrong Qt." >&2
    fi
    # linuxdeployqt scans the binary, follows Qt plugins (platforms/, imageformats/),
    # and bundles everything into DEPLOY_DIR.
    linuxdeployqt \
        "${DEPLOY_DIR}/ezspeccam" \
        "${QMAKE_FLAG[@]}" \
        -verbose=1
else
    echo "WARNING: linuxdeployqt not found in PATH." >&2
    echo "         Manual Qt runtime collection is not implemented; install linuxdeployqt" >&2
    echo "         (https://github.com/probonopd/linuxdeployqt) and re-run, or run" >&2
    echo "         ezspeccam from the build tree with the system Qt 6.2.4 runtime." >&2
fi


echo
echo "============================================"
echo "Deployment complete!"
echo "Output: ${DEPLOY_DIR}"
echo "============================================"
echo "Directory contents:"
ls -1 "$DEPLOY_DIR"
echo
echo "plugins/drivers:"
ls -1 "${DEPLOY_DIR}/plugins/drivers" 2>/dev/null || echo "(empty)"

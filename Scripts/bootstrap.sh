#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LILU_VERSION="${LILU_VERSION:-1.7.2}"

cd "$ROOT"

if ! command -v xcodebuild >/dev/null 2>&1; then
    echo "error: xcodebuild is unavailable; install full Xcode first" >&2
    exit 1
fi

developer_dir="$(xcode-select -p 2>/dev/null || true)"
if [ -z "$developer_dir" ] || [[ "$developer_dir" == *"CommandLineTools"* ]]; then
    echo "error: full Xcode is not selected" >&2
    echo "run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
fi

if [ ! -d MacKernelSDK ]; then
    git clone --depth 1 https://github.com/acidanthera/MacKernelSDK.git MacKernelSDK
fi

if [ ! -d Lilu.kext ]; then
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT

    archive="$tmp/Lilu.zip"
    url="https://github.com/acidanthera/Lilu/releases/download/${LILU_VERSION}/Lilu-${LILU_VERSION}-DEBUG.zip"

    echo "Downloading Lilu ${LILU_VERSION} DEBUG..."
    curl -LfsS "$url" -o "$archive"
    ditto -x -k "$archive" "$tmp/out"

    lilu="$(find "$tmp/out" -type d -name Lilu.kext -print -quit)"
    if [ -z "$lilu" ]; then
        echo "error: Lilu.kext not found in release archive" >&2
        exit 1
    fi

    ditto "$lilu" "$ROOT/Lilu.kext"
fi

for dependency in \
    Lilu.kext/Contents/Resources/Library/plugin_start.cpp \
    MacKernelSDK/Library/x86_64/libkmod.a; do
    if [ ! -e "$dependency" ]; then
        echo "error: missing dependency: $dependency" >&2
        exit 1
    fi
done

echo "Dependencies are ready."

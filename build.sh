#!/usr/bin/env bash
# Convenience wrapper: build with the VS-bundled CMake + vcpkg (Windows, Git Bash).
# Usage: ./build.sh [build-preset]   (default: win-x64-debug; see CMakePresets.json)
set -e
# Visual Studio: $VS_ROOT, else the latest install found by vswhere, else the default location
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
if [ -z "$VS_ROOT" ] && [ -x "$VSWHERE" ]; then
  VS_ROOT="$(cygpath -u "$("$VSWHERE" -latest -property installationPath | tr -d '')")"
fi
VS="${VS_ROOT:-/c/Program Files/Microsoft Visual Studio/2022/Community}"
export VCPKG_ROOT="${VCPKG_ROOT:-$VS/VC/vcpkg}"
CMAKE="$VS/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
PRESET="${1:-win-x64-debug}"
CONFIGURE_PRESET="${PRESET%-debug}"            # win-x64-debug -> win-x64, win-arm64-release -> win-arm64
CONFIGURE_PRESET="${CONFIGURE_PRESET%-release}"  # (win-x64-asan is both a build and a configure preset)
[ -f "build/$CONFIGURE_PRESET/CMakeCache.txt" ] || "$CMAKE" --preset "$CONFIGURE_PRESET"
"$CMAKE" --build --preset "$PRESET"

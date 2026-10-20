#!/usr/bin/env bash
# Build the AddressSanitizer configuration and run remc1 with the given arguments; print the first
# ASan report (stack + "located" line), or "no ASan errors".
# Windows (Git Bash) only for now: it uses the win-x64-asan preset. On Linux/macOS configure a build with
# -DCMAKE_CXX_FLAGS=-fsanitize=address and run remc1 directly (no preset yet).
#
# Usage: tools/asan_run.sh [--no-build] -- <remc1 args...>
#   e.g. tools/asan_run.sh -- --data <datadir> --input-script s.txt --exit-after-frames 1500
# This is the main tool for finding decompilation memory bugs.
REPO="$(cd "$(dirname "$0")/.." && pwd)"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) ;; *)
  echo "asan_run.sh: Windows only for now (win-x64-asan preset); see the header for Linux/macOS" >&2; exit 2;;
esac
BUILD=1
if [ "$1" == "--no-build" ]; then BUILD=0; shift; fi
[ "$1" == "--" ] && shift
if [ $BUILD == 1 ]; then
  (cd "$REPO" && ./build.sh win-x64-asan > "${TMPDIR:-/tmp}/remc1_asan_build.log" 2>&1) || {
    grep -E ": (fatal )?error" "${TMPDIR:-/tmp}/remc1_asan_build.log" | sort -u | head -20; exit 2; }
fi
LOG="${ASAN_RUN_LOG:-$(mktemp)}"
cd "$REPO/build/win-x64-asan/Debug" || exit 1
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" ./remc1.exe "$@" > "$LOG" 2>&1
CODE=$?
if grep -q "ERROR: AddressSanitizer" "$LOG"; then
  grep -A16 "ERROR: AddressSanitizer" "$LOG" | grep -v "^Shadow\|^  0x" | head -18
  grep -m1 -A1 "is located" "$LOG"
else
  echo "no ASan errors (exit code $CODE)"
  tail -3 "$LOG"
fi
echo "(log: $LOG)"

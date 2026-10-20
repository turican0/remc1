#!/usr/bin/env bash
# Run remc1 under a debugger and print a symbolised stack trace when it crashes or exits.
#   Windows (Git Bash): cdb from the Windows SDK (set CDB=<path> if it is not in the default place)
#   Linux / macOS:      gdb, else lldb
#
# Usage: tools/debug_run.sh [--build <dir>] -- <remc1 args...>
#   e.g. tools/debug_run.sh -- --data <datadir> --input-script s.txt --exit-after-frames 2000
# Without --build the newest build is used (tools/remc1_paths.py; $REMC1_EXE overrides).
#
# Notes (cdb):
#  * -hd disables the NT debug heap. Without it heap layout differs from a normal run and many
#    decompilation bugs (out-of-bounds reads into uninitialised memory) do not reproduce.
#  * Breakpoints on exit/_exit show who terminated the game (it calls exit() on many errors).
#  * The full debugger log path is printed at the end.
REPO="$(cd "$(dirname "$0")/.." && pwd)"
# first Python that really runs (on Windows "python3" may be the Microsoft Store stub)
PY="$PYTHON"
if [ -z "$PY" ]; then for p in python3 python; do "$p" -c "" 2> /dev/null && { PY="$p"; break; }; done; fi
EXE=""
if [ "$1" == "--build" ]; then
  for n in remc1.exe remc1; do [ -f "$2/$n" ] && EXE="$(cd "$2" && pwd)/$n"; done
  shift 2
fi
[ "$1" == "--" ] && shift
[ -n "$EXE" ] || EXE="$("$PY" "$REPO/tools/remc1_paths.py" exe)" || exit 1
cd "$(dirname "$EXE")" || exit 1
LOG="${DEBUG_RUN_LOG:-$(mktemp)}"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    # the debugger must match the executable (PE machine field: 0x8664 = x64, 0xAA64 = arm64)
    MACHINE="$("$PY" -c "import struct,sys; d=open(sys.argv[1],'rb').read(4096); print(hex(struct.unpack_from('<H', d, struct.unpack_from('<I', d, 0x3C)[0] + 4)[0]))" "$(cygpath -m "$EXE")")"
    case "$MACHINE" in 0x8664) ARCH=x64 ;; 0xaa64) ARCH=arm64 ;; *) ARCH=x86 ;; esac
    CDB="${CDB:-/c/Program Files (x86)/Windows Kits/10/Debuggers/$ARCH/cdb.exe}"
    if [ "$ARCH" = x86 ]; then
      CMDS="sxe av; bu ucrtbased!exit \"kP 30; q\"; bu ucrtbase!exit \"kP 30; q\"; g; .ecxr; kP 30; r; q"
    else
      # 64-bit ucrtbase exports exit/_exit/_Exit ambiguously for bu; abort() breaks into the debugger anyway
      CMDS="sxe av; g; .ecxr; kP 30; r; q"
    fi
    "$CDB" -hd -o -lines -c "$CMDS" "./$(basename "$EXE")" "$@" > "$LOG" 2>&1
    # show from the first exception/breakpoint onwards
    grep -v -e "^ModLoad" -e "NatVis" -e "^\*\*\*\*" -e "Symbol search path" -e "^$" "$LOG" \
      | sed -n -e '/violation\|Breakpoint\|exception -\|Privileged\|Illegal\|ChildEBP/,$p' | head -120
    ;;
  *)
    if command -v gdb > /dev/null; then
      gdb -batch -ex run -ex bt -ex "info registers" --args "$EXE" "$@" > "$LOG" 2>&1
    elif command -v lldb > /dev/null; then
      lldb --batch -o run -k bt -k "register read" -- "$EXE" "$@" > "$LOG" 2>&1
    else
      echo "neither gdb nor lldb found" >&2; exit 1
    fi
    sed -n -e '/signal\|stop reason\|exited with/,$p' "$LOG" | head -120
    ;;
esac
echo "(full log: $LOG)"

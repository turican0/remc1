#!/usr/bin/env python3
"""Where the tools find the built port, the game data and the original executable - on any OS.

  port executable  $REMC1_EXE, else the newest remc1 / remc1.exe under build/<preset>/ (multi-config
                   generators: build/<preset>/<Config>/; Ninja/Make: build/<preset>/), release builds first
  game data        $REMC1_DATA_DIR, else C:/mc1data on Windows and ~/mc1data elsewhere
  CARPET.EXE       <data>/CD/CARPET.EXE (the original DOS executable, used by the diff harness)

  python tools/remc1_paths.py exe     # print the port executable (for shell scripts)
  python tools/remc1_paths.py data    # print the data folder
"""
import glob
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_NAME = "remc1.exe" if os.name == "nt" else "remc1"
# Search order when several builds exist: release flavours first, then debug; newest wins within a rank.
CONFIG_RANK = {"relwithdebinfo": 0, "release": 1, "minsizerel": 2, "debug": 3}


def default_data_dir():
    env = os.environ.get("REMC1_DATA_DIR")
    if env:
        return env
    return "C:/mc1data" if os.name == "nt" else os.path.join(os.path.expanduser("~"), "mc1data")


def original_exe(data=None):
    return os.path.join(data or default_data_dir(), "CD", "CARPET.EXE")


def _rank(path):
    parts = [p.lower() for p in os.path.normpath(path).split(os.sep)]
    preset = parts[-2] if len(parts) >= 2 else ""
    config = CONFIG_RANK.get(parts[-2], CONFIG_RANK.get("debug" if "debug" in preset else "release"))
    sanitizer = 1 if "asan" in "".join(parts) else 0  # instrumented builds last
    return (sanitizer, config, -os.path.getmtime(path))


def find_port_exe(required=True):
    """The built port executable, or None (required=False) / SystemExit when there is none."""
    env = os.environ.get("REMC1_EXE")
    if env:
        return env
    cands = [p for pat in (os.path.join(ROOT, "build", "*", EXE_NAME), os.path.join(ROOT, "build", "*", "*", EXE_NAME))
             for p in glob.glob(pat) if os.path.isfile(p)]
    if cands:
        return min(cands, key=_rank)
    if required:
        raise SystemExit(f"no {EXE_NAME} under {os.path.join(ROOT, 'build')} - build first (docs/building-<platform>.md) "
                         "or set REMC1_EXE / pass --exe")
    return None


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "exe"
    print({"exe": lambda: find_port_exe(), "data": default_data_dir, "original": original_exe}[what]())

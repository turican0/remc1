#!/usr/bin/env python3
"""Record and play bug recordings as bundles (<name>.recording/ = input.txt + saves/, see recording.py).

  python tools/rec.py record recordings/castle_bug            # play normally; quit the game to stop
  python tools/rec.py play   recordings/castle_bug.recording  # watch it at normal speed
  python tools/rec.py play   old_recording.txt --keep-saves      # legacy .txt: use the install's saves

record: copies <data>/CARPET.CD/SAVE into the bundle first (the saves the session starts from), then
        runs remc1 on the real install with --record-input. Saving/loading during the session works
        as usual.
play:   builds a temporary C: drive (install without saves + the bundle's saves) and runs the
        recording through --c-drive / --input-script; live input is ignored while it plays. Prints the
        first "DESYNC at frame N" if the game state drifts from the recording.

Options: --data DIR (default $REMC1_DATA_DIR, else C:/mc1data on Windows, ~/mc1data elsewhere), --exe
PATH (default $REMC1_EXE, else the newest build under build/, see remc1_paths.py), --headless (play
only). Arguments after
"--" go to remc1.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recording  # noqa: E402
import remc1_paths  # noqa: E402


def run_game(cmd, exe):
    """Run remc1, echo its output, return (exit code, first DESYNC line or None)."""
    desync = None
    p = subprocess.Popen(cmd, cwd=os.path.dirname(os.path.abspath(exe)), stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    for raw in p.stdout:
        line = raw.decode("latin-1", "replace").rstrip()
        print(line)
        if desync is None and "DESYNC" in line:
            desync = line
    return p.wait(), desync


def record(a, extra):
    bundle = recording.bundle_path(a.name)
    if os.path.exists(bundle) and not a.force:
        raise SystemExit(f"{bundle} exists (use --force to replace it)")
    shutil.rmtree(bundle, ignore_errors=True)
    os.makedirs(bundle)
    saves = recording.install_saves(a.data)
    recording.copy_saves(saves, os.path.join(bundle, recording.SAVES))
    n = len(os.listdir(os.path.join(bundle, recording.SAVES)))
    print(f"{bundle}: {n} saved game file(s) copied from {saves}")
    script = os.path.join(bundle, recording.INPUT)
    cmd = [os.path.abspath(a.exe), "--data", a.data, "--record-input", os.path.abspath(script)] + extra
    rc, _ = run_game(cmd, a.exe)
    print(f"recording finished (exit code {rc}): {bundle}")
    return rc


def play(a, extra):
    script, saves = recording.resolve(a.name)
    if saves is None and a.keep_saves:
        saves = recording.install_saves(a.data)
    with tempfile.TemporaryDirectory(prefix="remc1_play_") as cdrive:
        recording.build_c_drive(a.data, cdrive, saves)
        cmd = [os.path.abspath(a.exe), "--data", a.data, "--c-drive", cdrive,
               "--input-script", os.path.abspath(script)]
        if a.headless:
            cmd.append("--headless")
        rc, desync = run_game(cmd + extra, a.exe)
    print(f"exit code {rc}; " + (desync or "no desync reported"))
    return rc


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["record", "play"])
    ap.add_argument("name", help="bundle (<name>[.recording]) or, for play, a legacy .txt recording")
    ap.add_argument("--data", default=recording.DEFAULT_DATA)
    ap.add_argument("--exe")
    ap.add_argument("--force", action="store_true", help="record: replace an existing bundle")
    ap.add_argument("--headless", action="store_true", help="play: no window")
    ap.add_argument("--keep-saves", action="store_true", help="play a legacy .txt with the install's current saves")
    a, extra = ap.parse_known_args()
    extra = [x for x in extra if x != "--"]
    a.exe = a.exe or remc1_paths.find_port_exe()
    return record(a, extra) if a.command == "record" else play(a, extra)


if __name__ == "__main__":
    sys.exit(main())

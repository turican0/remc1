#!/usr/bin/env python3
"""Regression check over recording bundles: replays each one headless and compares a per-frame trace of
the whole game state and the presented image against the trace stored in the bundle.

    python tools/regress.py                       # every bundle in <data>/recordings
    python tools/regress.py path/to/level3.recording other.recording
    python tools/regress.py --update              # (re)write <bundle>/trace.txt from this build

The trace (remc1 --state-trace) has one line per presented frame: "<frame> <state hash> <image hash>".
The state hash covers every byte of the game state with pointer slots normalised (Port_FullStateHash),
the image hash the indexed frame and palette - both independent of pointer size and platform, so traces
written by one build verify any other (32-bit vs 64-bit, MSVC vs GCC/Clang, x64 vs arm64).

A bundle's trace.txt is written only with --update; input.txt and saves/ are never touched.
Options: --data DIR (default $REMC1_DATA_DIR, else C:/mc1data on Windows, ~/mc1data elsewhere),
--exe PATH (default $REMC1_EXE, else the newest build), -j N parallel runs.
"""
import argparse
import concurrent.futures
import glob
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recording  # noqa: E402  (bundles, C: drive)
import remc1_paths  # noqa: E402

TRACE = "trace.txt"


def read_trace(path):
    out = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 3:
                out.append((int(p[0]), p[1], p[2]))
    return out


def run(bundle, a):
    """Replay one bundle; returns (bundle, trace lines, recording desync line or None, exit code)."""
    script, saves = recording.resolve(bundle)
    tmp = tempfile.mkdtemp(prefix="remc1_regress_")
    try:
        cdrive = os.path.join(tmp, "cdrive")
        os.makedirs(cdrive)
        recording.build_c_drive(a.data, cdrive, saves)
        trace = os.path.join(tmp, TRACE)
        cmd = [os.path.abspath(a.exe), "--headless", "--data", a.data, "--c-drive", cdrive,
               "--input-script", os.path.abspath(script), "--state-trace", trace]
        r = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(a.exe)), capture_output=True,
                           env=dict(os.environ, SDL_AUDIODRIVER="dummy"))
        log = (r.stdout + r.stderr).decode("latin-1", "replace").splitlines()
        desync = next((l.strip() for l in log if "DESYNC" in l), None)
        lines = read_trace(trace) if os.path.exists(trace) else []
        return bundle, lines, desync, r.returncode
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def compare(ref, got):
    """First frame whose state / image hash differs (None = identical), and a length note."""
    first_state = first_image = None
    for (f, s, i), (g, t, j) in zip(ref, got):
        if f != g:
            return f, f, f"frame numbering differs at {f}/{g}"
        if first_state is None and s != t:
            first_state = f
        if first_image is None and i != j:
            first_image = f
    note = "" if len(ref) == len(got) else f"length {len(got)} frames, reference {len(ref)}"
    return first_state, first_image, note


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bundles", nargs="*", help="recording bundles (default: every *.recording in <data>/recordings)")
    ap.add_argument("--data", default=remc1_paths.default_data_dir())
    ap.add_argument("--exe")
    ap.add_argument("--update", action="store_true", help="write <bundle>/trace.txt from this build")
    ap.add_argument("-j", type=int, default=max(1, (os.cpu_count() or 2) // 2), help="parallel runs")
    a = ap.parse_args()
    a.exe = a.exe or remc1_paths.find_port_exe()
    if not os.path.isfile(a.exe):
        print(f"{a.exe}: no such executable")
        return 1
    bundles = a.bundles or sorted(glob.glob(os.path.join(a.data, "recordings", "*" + recording.SUFFIX)),
                                  key=lambda p: (len(p), p))
    if not bundles:
        print(f"no recording bundles in {os.path.join(a.data, 'recordings')}")
        return 1
    print(f"exe: {a.exe}")
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(a.j) as pool:
        for bundle, lines, desync, rc in pool.map(lambda b: run(b, a), bundles):
            name = os.path.basename(bundle.rstrip("/\\"))
            info = f" [{desync}]" if desync else ""
            if rc != 0 or not lines:
                print(f"{name}: FAILED (exit code {rc & 0xFFFFFFFF:#x}, {len(lines)} frames){info}")
                failed += 1
                continue
            ref_path = os.path.join(bundle, TRACE)
            if a.update:
                with open(ref_path, "w") as f:
                    f.writelines(f"{fr} {s} {i}\n" for fr, s, i in lines)
                print(f"{name}: trace written ({len(lines)} frames){info}")
                continue
            if not os.path.exists(ref_path):
                print(f"{name}: no {TRACE} (run with --update on a reference build)")
                failed += 1
                continue
            s, i, note = compare(read_trace(ref_path), lines)
            if s is None and i is None and not note:
                print(f"{name}: OK ({len(lines)} frames){info}")
            else:
                failed += 1
                parts = [f"state differs from frame {s}" if s is not None else "",
                         f"image differs from frame {i}" if i is not None else "", note]
                print(f"{name}: DIFFERS - " + "; ".join(p for p in parts if p) + info)
    print(f"{len(bundles) - failed}/{len(bundles)} bundles OK" if not a.update else "")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

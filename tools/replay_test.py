#!/usr/bin/env python3
"""Tier-3 replay test: run remc1 headless + deterministic with a committed input script and compare
the per-frame framebuffer hashes against a golden file.

Usage:
  python tools/replay_test.py [--exe <built remc1>] --replay tests/replays/boot_intro.txt
         [--data <datadir>] [--update] [--keep]

Replay files are ordinary input scripts (portability/InputScript.h). Lines starting with "#!" add
command-line options, e.g. "#! --exit-after-frames 600 --deterministic 5".
Goldens: tests/goldens/replay_<name>.txt. Regenerate with --update ONLY from a build whose behaviour
you trust.

Each run gets a fresh copy of <data>/CARPET.CD (the DOS C: drive) with saves removed and
language.inf set to English, so results do not depend on earlier play sessions.
Exit codes: 0 pass, 1 fail, 77 skipped (no game data).
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remc1_paths  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = 77


def find_ci(base, name):
    for n in os.listdir(base):
        if n.upper() == name.upper():
            return os.path.join(base, n)
    return None


def make_c_drive(data, dest):
    src = find_ci(data, "CARPET.CD")
    if not src:
        raise SystemExit(f"{data} has no CARPET.CD folder")
    target = os.path.join(dest, "CARPET.CD")
    shutil.copytree(src, target)
    save = find_ci(target, "SAVE")
    if save:
        shutil.rmtree(save)
    os.makedirs(os.path.join(target, "SAVE"))
    for n in os.listdir(target):
        if n.upper() == "LANGUAGE.INF":
            os.remove(os.path.join(target, n))
    with open(os.path.join(target, "language.inf"), "wb") as f:
        f.write(b"\x00")  # English: skips the language book


def read_hashes(path):
    out = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                out.append(line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", help="built port executable (default: $REMC1_EXE or the newest build)")
    ap.add_argument("--replay", required=True)
    ap.add_argument("--data", default=os.environ.get("REMC1_DATA_DIR", ""))
    ap.add_argument("--update", action="store_true", help="rewrite the golden file")
    ap.add_argument("--keep", action="store_true", help="keep the temp folder (prints its path)")
    ap.add_argument("--timeout", type=int, default=900)
    args = ap.parse_args()
    args.exe = args.exe or remc1_paths.find_port_exe()

    if not args.data or not os.path.isdir(os.path.join(args.data, "CD")):
        print("SKIP: set REMC1_DATA_DIR (or --data) to the extracted game data")
        return SKIP

    name = os.path.splitext(os.path.basename(args.replay))[0]
    golden = os.path.join(ROOT, "tests", "goldens", f"replay_{name}.txt")
    extra = []
    with open(args.replay) as f:
        for line in f:
            if line.startswith("#!"):
                extra += line[2:].split()

    tmp = tempfile.mkdtemp(prefix=f"remc1_replay_{name}_")
    try:
        make_c_drive(args.data, tmp)
        hashes = os.path.join(tmp, "frames.txt")
        cmd = [os.path.abspath(args.exe), "--headless", "--data", args.data, "--c-drive", tmp,
               "--input-script", os.path.abspath(args.replay), "--frame-hashes", hashes] + extra
        if "--deterministic" not in extra:
            cmd += ["--deterministic", "5"]
        env = dict(os.environ, SDL_AUDIODRIVER="dummy")
        proc = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(args.exe)), env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=args.timeout)
        log = proc.stdout.decode("latin-1", "replace")
        if proc.returncode != 0:
            print(log[-3000:])
            print(f"FAIL: remc1 exited with {proc.returncode}")
            return 1
        got = read_hashes(hashes)
        if args.update:
            with open(golden, "w", newline="\n") as f:
                f.write(f"# replay {name}: frame WxH fnv1a64(framebuffer+palette); regenerate with tools/replay_test.py --update\n")
                f.write("\n".join(got) + "\n")
            print(f"updated {golden} ({len(got)} frames)")
            return 0
        if not os.path.exists(golden):
            print(f"FAIL: no golden {golden}; run with --update on a trusted build")
            return 1
        want = read_hashes(golden)
        for i, (g, w) in enumerate(zip(got, want)):
            if g != w:
                print(f"FAIL: first difference at line {i + 1}:\n  got      {g}\n  expected {w}")
                return 1
        if len(got) != len(want):
            print(f"FAIL: {len(got)} frames, golden has {len(want)}")
            return 1
        print(f"PASS: {len(got)} frames identical")
        return 0
    finally:
        if args.keep:
            print("kept", tmp)
        else:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())

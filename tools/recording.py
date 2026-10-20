"""Recording bundles: one folder per bug recording, so a replay starts from the same saved games.

    <name>.recording/
        input.txt   input recording (portability/InputScript.h format, made with --record-input)
        saves/      copy of <data>/CARPET.CD/SAVE taken when the recording session started

Legacy recordings are a bare <name>.txt without saves. Used by tools/rec.py and tools/run_recording.py.
"""
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay_test  # noqa: E402  (make_c_drive, find_ci)
import remc1_paths  # noqa: E402  (find_port_exe, default_data_dir)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DATA = remc1_paths.default_data_dir()
SUFFIX = ".recording"
INPUT = "input.txt"
SAVES = "saves"


def bundle_path(name):
    """<name> or <name>.recording -> <name>.recording"""
    name = name.rstrip("/\\")
    return name if name.endswith(SUFFIX) else name + SUFFIX


def resolve(path):
    """A bundle folder or a legacy .txt -> (input script, saves folder or None)."""
    if os.path.isdir(path):
        script = os.path.join(path, INPUT)
        if not os.path.isfile(script):
            raise SystemExit(f"{path}: no {INPUT} in this recording bundle")
        saves = os.path.join(path, SAVES)
        return script, (saves if os.path.isdir(saves) else None)
    if os.path.isfile(path):
        return path, None
    if os.path.isdir(bundle_path(path)):
        return resolve(bundle_path(path))
    raise SystemExit(f"{path}: no such recording (bundle folder or .txt)")


def install_saves(data):
    """<data>/CARPET.CD/SAVE (the live install's saves) or None."""
    cd = replay_test.find_ci(data, "CARPET.CD")
    return replay_test.find_ci(cd, "SAVE") if cd else None


def copy_saves(src, dst):
    os.makedirs(dst, exist_ok=True)
    if src and os.path.isdir(src):
        for n in os.listdir(src):
            p = os.path.join(src, n)
            if os.path.isfile(p):
                shutil.copy2(p, dst)


def build_c_drive(data, dest, saves=None):
    """Fresh DOS C: drive in dest (a CARPET.CD copy without saves, see replay_test.make_c_drive),
    then the given saves folder copied into CARPET.CD/SAVE."""
    replay_test.make_c_drive(data, dest)
    copy_saves(saves, os.path.join(dest, "CARPET.CD", "SAVE"))

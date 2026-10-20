#!/usr/bin/env python3
"""Extract Magic Carpet 1 game data from a GOG install into the REMC1 data layout.

Usage:
    python tools/extract_data.py --gog "<GOG Magic Carpet Plus folder>" --out <datadir>

Layout produced:
    <out>/CD/          contents of the CD's CARPET\\ folder (read-only game data: DATA, LEVELS, INTRO, ...)
    <out>/CARPET.CD/   the writable "C:\\CARPET.CD" install folder (SAVE, DATA tmaps, LEVELS, setup files)
    <out>/manifest.sha256

Only the Python standard library is used. game.gog is a plain ISO9660 image with 2048-byte sectors,
so a minimal directory-record walker is enough (no Joliet / Rock Ridge needed).
"""
import argparse
import hashlib
import os
import shutil
import struct
import sys

SECTOR = 2048


class Iso9660:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.f.seek(16 * SECTOR)
        pvd = self.f.read(SECTOR)
        if pvd[0] != 1 or pvd[1:6] != b"CD001":
            raise ValueError(f"{path}: no ISO9660 primary volume descriptor at sector 16")
        self.root = self._parse_record(pvd[156:156 + 34])

    @staticmethod
    def _parse_record(rec):
        length = rec[0]
        extent = struct.unpack_from("<I", rec, 2)[0]
        size = struct.unpack_from("<I", rec, 10)[0]
        flags = rec[25]
        name_len = rec[32]
        name = rec[33:33 + name_len]
        return {"len": length, "extent": extent, "size": size, "dir": bool(flags & 2), "name": name}

    def listdir(self, entry):
        self.f.seek(entry["extent"] * SECTOR)
        data = self.f.read(entry["size"])
        out = []
        pos = 0
        while pos < len(data):
            length = data[pos]
            if length == 0:
                # records never straddle sectors; skip to the next sector
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            rec = self._parse_record(data[pos:pos + length])
            pos += length
            if rec["name"] in (b"\x00", b"\x01"):
                continue  # "." and ".."
            name = rec["name"].decode("ascii")
            if not rec["dir"]:
                name = name.split(";")[0].rstrip(".")
            rec["name"] = name
            out.append(rec)
        return out

    def find(self, path):
        entry = self.root
        for part in [p for p in path.replace("\\", "/").split("/") if p]:
            match = [e for e in self.listdir(entry) if e["name"].upper() == part.upper()]
            if not match:
                raise FileNotFoundError(path)
            entry = match[0]
        return entry

    def extract_tree(self, entry, dest):
        os.makedirs(dest, exist_ok=True)
        for e in self.listdir(entry):
            target = os.path.join(dest, e["name"])
            if e["dir"]:
                self.extract_tree(e, target)
            else:
                self.f.seek(e["extent"] * SECTOR)
                remaining = e["size"]
                with open(target, "wb") as out:
                    while remaining:
                        chunk = self.f.read(min(remaining, 1 << 20))
                        out.write(chunk)
                        remaining -= len(chunk)


# Loose files GOG ships in CARPET.CD (the DOS "C:\CARPET.CD" install dir). Everything else in that
# folder (third-party tools, unpacked levels, ...) is ignored.
INSTALL_FILES = [
    "CP.DAT", "INTRO.PLD", "SNDSETUP.DAT", "SNDSETUP.INF",
    "DATA/TMAPS.DAT", "DATA/TMAPS.TAB", "DATA/TMAPS0-0.DAT", "DATA/TMAPS0-0.TAB",
    "DATA/TMAPS1-0.DAT", "DATA/TMAPS1-0.TAB",
]
# Level files are taken from the CD originals: GOG installs are sometimes modified by level editors.
LEVEL_FILES = ["LEVELS.DAT", "LEVELS.TAB", "DDLEVELS.DAT", "DDLEVELS.TAB"]


def find_ci(base, rel):
    """Case-insensitive lookup of rel under base; returns real path or None."""
    cur = base
    for part in rel.replace("\\", "/").split("/"):
        try:
            names = os.listdir(cur)
        except OSError:
            return None
        hit = [n for n in names if n.upper() == part.upper()]
        if not hit:
            return None
        cur = os.path.join(cur, hit[0])
    return cur


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_manifest(out):
    lines = []
    for sub in ("CD", "CARPET.CD"):
        root = os.path.join(out, sub)
        for dirpath, _, files in os.walk(root):
            for name in files:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, out).replace("\\", "/")
                lines.append(f"{sha256(full)}  {rel}")
    lines.sort(key=lambda l: l.split("  ", 1)[1].upper())
    with open(os.path.join(out, "manifest.sha256"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return lines


def verify(lines, reference):
    if not os.path.exists(reference):
        print(f"note: no reference manifest at {reference}; skipping verification")
        return True
    ref = {}
    with open(reference) as f:
        for line in f:
            if line.strip():
                digest, rel = line.rstrip("\n").split("  ", 1)
                ref[rel.upper()] = digest
    got = {l.split("  ", 1)[1].upper(): l.split("  ", 1)[0] for l in lines}
    ok = True
    for rel, digest in ref.items():
        if rel not in got:
            print(f"MISSING  {rel}")
            ok = False
        elif got[rel] != digest:
            print(f"MISMATCH {rel}")
            ok = False
    print("manifest verification:", "OK" if ok else "FAILED (different game version or modified files?)")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gog", required=True, help="GOG 'Magic Carpet Plus' install folder (contains CARPET.CD/game.gog)")
    ap.add_argument("--out", required=True, help="output data folder")
    ap.add_argument("--write-reference", action="store_true",
                    help="(maintainers) overwrite tools/data_manifest.sha256 with this extraction's hashes")
    args = ap.parse_args()

    carpet_cd = find_ci(args.gog, "CARPET.CD")
    image = carpet_cd and find_ci(carpet_cd, "game.gog")
    if not image:
        sys.exit(f"error: {args.gog}/CARPET.CD/game.gog not found")

    out = os.path.abspath(args.out)
    cd_out = os.path.join(out, "CD")
    inst_out = os.path.join(out, "CARPET.CD")

    print(f"reading {image}")
    iso = Iso9660(image)
    if os.path.exists(cd_out):
        shutil.rmtree(cd_out)
    iso.extract_tree(iso.find("CARPET"), cd_out)
    print(f"extracted CD data -> {cd_out}")

    os.makedirs(os.path.join(inst_out, "SAVE"), exist_ok=True)
    os.makedirs(os.path.join(inst_out, "DATA"), exist_ok=True)
    os.makedirs(os.path.join(inst_out, "LEVELS"), exist_ok=True)
    for rel in INSTALL_FILES:
        src = find_ci(carpet_cd, rel)
        if src:
            shutil.copyfile(src, os.path.join(inst_out, *rel.split("/")))
        else:
            print(f"warning: install file {rel} not found in {carpet_cd}")
    for name in LEVEL_FILES:
        shutil.copyfile(os.path.join(cd_out, "LEVELS", name), os.path.join(inst_out, "LEVELS", name))
    print(f"seeded install folder -> {inst_out}")

    lines = write_manifest(out)
    reference = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data_manifest.sha256")
    if args.write_reference:
        # SAVE/ and generated files are excluded; only the shipped data is a reference.
        with open(reference, "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        print(f"wrote reference manifest {reference}")
    elif not verify(lines, reference):
        sys.exit(1)
    print("done. Run: remc1 --data", out)


if __name__ == "__main__":
    main()

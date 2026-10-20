#!/usr/bin/env python3
"""Tile frame dumps (from remc1 --dump-frames) into one PNG so a whole run can be inspected at once.

Usage: python tools/contact_sheet.py <frames-dir> [--out sheet.png] [--cols 4] [--step 1] [--last N]
Each tile is labelled with its frame number. Requires Pillow.
"""
import argparse
import glob
import os
import re

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--out", default=None)
    ap.add_argument("--cols", type=int, default=4)
    ap.add_argument("--step", type=int, default=1, help="use every Nth dump")
    ap.add_argument("--last", type=int, default=0, help="only the last N (after --step)")
    ap.add_argument("--width", type=int, default=320, help="tile width")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.dir, "frame*.bmp")))[::args.step]
    if args.last:
        files = files[-args.last:]
    if not files:
        raise SystemExit("no frame*.bmp in " + args.dir)
    tw = args.width
    th = tw * 3 // 4  # every VGA mode was shown 4:3
    rows = (len(files) + args.cols - 1) // args.cols
    sheet = Image.new("RGB", (tw * args.cols, th * rows))
    draw = ImageDraw.Draw(sheet)
    for i, f in enumerate(files):
        im = Image.open(f).convert("RGB").resize((tw, th))
        x, y = (i % args.cols) * tw, (i // args.cols) * th
        sheet.paste(im, (x, y))
        label = re.sub(r"\D", "", os.path.basename(f)).lstrip("0") or "0"
        draw.rectangle([x, y, x + 8 * len(label) + 4, y + 12], fill=(0, 0, 0))
        draw.text((x + 2, y), label, fill=(255, 255, 0))
    out = args.out or os.path.join(args.dir, "sheet.png")
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()

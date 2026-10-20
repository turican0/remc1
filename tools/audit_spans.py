#!/usr/bin/env python3
"""Port globals that the harness cuts short: tools/emu.py lays address-named globals out linker-style (a
global ends where the next one starts), so a global whose original address lies inside another global's
extent ends that one there. Intended for enlarged placeholder arrays; harmful for a stray declaration
named into a real table (byte_B5D41 cut the 4802-byte byte_B5D40_B5D30 to 1 byte). Lines with
"0 mentions" name a cutting global the code never uses - delete that declaration.

Usage (from the repo root, after a build): python tools/audit_spans.py [port exe]
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remc1_paths  # noqa: E402
from audit_data import name_addresses  # noqa: E402
from le_image import LeImage  # noqa: E402
from port_symbols import PortSymbols  # noqa: E402


def main():
    img = LeImage(remc1_paths.original_exe())
    ps = PortSymbols(sys.argv[1] if len(sys.argv) > 1 else remc1_paths.find_port_exe())
    src = open('sub_main.cpp', 'rb').read().decode('latin-1')
    spans = sorted((name_addresses(s.name, img)[0], s.size, s.name) for s in ps.data_symbols()
                   if s.size and name_addresses(s.name, img))
    for i, (a, n, nm) in enumerate(spans):
        for b, m, nm2 in spans[i + 1:]:
            if b >= a + n:
                break
            if b + m <= a + n:  # entirely inside: IDA junk or a struct member, not a boundary
                continue
            uses = len(re.findall(r'\b' + re.escape(nm2) + r'\b', src))
            print(f'{nm} @{a:X} size {n} cut to {b - a} by {nm2} @{b:X} (size {m}, {uses} mentions)')
            break


if __name__ == '__main__':
    main()

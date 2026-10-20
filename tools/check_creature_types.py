#!/usr/bin/env python3
"""Cross-check every creatureTypes_98F38[N] (was unk_98F38) (creature Type_156 profile) use in sub_main.cpp against the
type-pointer immediates in the original CARPET.EXE. Lines marked CHECK need a look in the disassembly.

Usage (from the repo root): python tools/check_creature_types.py [--exe <data>/CD/CARPET.EXE]
"""
import re
import os
import remc1_paths  # noqa: E402
import struct
import sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from exe_data import LeImage

img = LeImage(sys.argv[sys.argv.index('--exe') + 1] if '--exe' in sys.argv else remc1_paths.original_exe())
base, vsize, pidx, pcnt = img.objects[0]
code = img.read(base, min(vsize, pcnt * img.page_size))
BASE_T = 0x8F38
hits = []  # (addr, index)
for i in range(len(code) - 4):
    v = struct.unpack_from('<I', code, i)[0]
    if BASE_T <= v < BASE_T + 31 * 32 and (v - BASE_T) % 32 == 0:
        hits.append((base + i, (v - BASE_T) // 32))

lines = open('sub_main.cpp', 'rb').read().decode('latin-1').splitlines()
func_re = re.compile(r'^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;]*$')
addr_re = re.compile(r'_([0-9A-F]{5})(?:_[0-9A-F]{5})?$')
cur = None
uses = {}
for n, l in enumerate(lines, 1):
    m = func_re.match(l)
    if m and not l.startswith(' ') and '=' not in l:
        name = m.group(1)
        a = addr_re.search(name)
        cur = (name, int(a.group(1), 16) if a else None)
    for idx in re.findall(r'(?:creatureTypes|unk)_98F38\[([^\]]+)\]', l):
        if cur and not l.lstrip().startswith('//'):
            uses.setdefault(cur, []).append((n, idx))

starts = sorted(a for (_, a) in uses if a)
for (name, a), us in sorted(uses.items(), key=lambda x: x[0][1] or 0):
    if a is None:
        print(f'{name}: no address; uses {us}')
        continue
    # asm hits within this function: up to 0x400 bytes, stopping at the next known function start
    nxt = min([s for s in starts if s > a] + [a + 0x400])
    asm = sorted({idx for (h, idx) in hits if a <= h < nxt})
    port = [u[1] for u in us]
    flag = '' if all(p.isdigit() and int(p) in asm for p in port) and asm else '   <-- CHECK'
    print(f'{name} @{a:#x}: port {port} asm {asm}{flag}')

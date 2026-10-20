#!/usr/bin/env python3
"""List uses of 8/16-bit struct fields with constants that cannot fit the field (no-op |=, never-true &).

Example found: triangleFeatures_38 (uint16) |= 0x100000. Fields are taken from the struct declarations in
engine/*.h and sub_main.cpp; uses are `.field OP const` / `->field OP const` with OP one of |= &= ^= = & |.

    python tools/scan_wide_consts.py
"""
import glob
import re

WIDTH = {'uint8': 8, 'uint8_t': 8, 'int8': 8, 'int8_t': 8, 'char': 8, '__int8': 8, '_BYTE': 8,
         'uint16': 16, 'uint16_t': 16, 'int16': 16, 'int16_t': 16, '__int16': 16, '_WORD': 16}


def main():
    fields = {}
    for p in glob.glob('engine/*.h') + ['sub_main.cpp']:
        s = open(p, 'rb').read().decode('latin-1')
        for m in re.finditer(r'^\s*(?:unsigned\s+|signed\s+)?(\w+)\s+(\w+)\s*;', s, re.M):
            t, n = m.group(1), m.group(2)
            if t in WIDTH and not n.startswith(('v', 'result')):
                fields.setdefault(n, set()).add(WIDTH[t])
    s = open('sub_main.cpp', 'rb').read().decode('latin-1')
    names = '|'.join(re.escape(n) for n in fields if len(fields[n]) == 1)
    pat = re.compile(r'(?:\.|->)(' + names + r')\s*(\|=|&=|\^=|=|&|\|)\s*\(?\s*(0x[0-9A-Fa-f]+|\d+)u?\b')
    hits = 0
    for i, line in enumerate(s.split('\n'), 1):
        if line.strip().startswith('//'):
            continue
        for m in pat.finditer(line):
            n, op, c = m.group(1), m.group(2), int(m.group(3), 0)
            w = next(iter(fields[n]))
            if c >= (1 << w) and op != '=':
                hits += 1
                print(f'sub_main.cpp:{i}: {n} ({w}-bit) {op} {m.group(3)}')
    print(f'# {hits} suspicious uses')


if __name__ == '__main__':
    main()

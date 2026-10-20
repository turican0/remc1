#!/usr/bin/env python3
"""Find entity fields whose signedness in the port differs from how the original code widens them.

The struct conversion of the decompiled code typed some fields unsigned where the original loads them
with movsx (or the other way round). Where the value then goes through a sign-sensitive operation (a
difference compared with a limit, a division rounding toward zero, ...) the port behaves differently for
values >= 0x8000 (found: axis_3d.x/.y in sub_120B0 and UpdateFollower_1A390).

Fields come from the entity struct in engine/Basic.h: every 8/16-bit member whose name ends in its
entity offset (..._29867_72) plus axis_3d x/y/z at +72/74/76. For every port function with an original
address that names such a field, the original function is walked and its loads of [reg + offset] of the
field's size are classified movsx / movzx. Prints, per field, the functions where the original widens
the field with the other signedness than the port's type (and whether the port casts there).

    python tools/audit_signedness.py [--field NAME] [--verbose]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import x86
from orig_code import OrigCode
from port_index import PortIndex

TYPES = {'uint8': (1, False), 'uint8_t': (1, False), 'int8': (1, True), 'int8_t': (1, True), 'char': (1, True),
         'uint16': (2, False), 'uint16_t': (2, False), 'int16': (2, True), 'int16_t': (2, True)}


def entity_fields(path='engine/Basic.h'):
    """{name: (offset, size, signed)}"""
    out = {}
    for m in re.finditer(r'^\s*(\w+)\s+(\w+_\d{5}_(\d+))\s*;', open(path).read(), re.M):
        t, name, off = m.group(1), m.group(2), int(m.group(3))
        if t in TYPES:
            out[name] = (off,) + TYPES[t]
    out['var_u32_29867_72.x'] = (72, 2, False)
    out['var_u32_29867_72.y'] = (74, 2, False)
    out['var_u32_29867_72.z'] = (76, 2, True)
    return out


def widening(oc, start):
    """{(disp, size): set('sx'|'zx')} for widening loads [reg + disp] in the original function."""
    out = {}
    for a, ins in oc.walk(start)['insns'].items():
        if ins.mnemonic not in ('movsx', 'movzx') or len(ins.operands) != 2:
            continue
        src = ins.operands[1]
        if src.type != x86.X86_OP_MEM or src.mem.base == 0 or src.mem.index != 0:
            continue
        out.setdefault((src.mem.disp, src.size), set()).add('sx' if ins.mnemonic == 'movsx' else 'zx')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--field', help='only this field')
    ap.add_argument('--verbose', action='store_true', help='list the functions, not just counts')
    a = ap.parse_args()
    fields = entity_fields()
    if a.field:
        fields = {a.field: fields[a.field]}
    oc = OrigCode()
    idx = PortIndex('sub_main.cpp')
    starts = {f['addr'] for f in idx.funcs.values() if f['addr'] and oc.in_code(f['addr'])}
    oc.known_starts = starts
    pats = {n: re.compile(r'(\(\s*(?:signed |unsigned )?(?:__int16|__int8|char|int16|uint16|int8|uint8)(?:_t)?\s*\)\s*)?'
                          r'[\w\]\[]*(?:->|\.)' + re.escape(n) + r'\b') for n in fields}
    report = {}
    for name, f in sorted(idx.funcs.items(), key=lambda kv: kv[1]['addr'] or 0):
        addr = f['addr']
        if addr not in starts:
            continue
        used = {n: p.findall(f['body']) for n, p in pats.items()}
        used = {n: u for n, u in used.items() if u}
        if not used:
            continue
        wid = widening(oc, addr)
        for n, uses in used.items():
            off, size, signed = fields[n]
            k = wid.get((off, size), set())
            want = 'zx' if signed else 'sx'
            if want in k and len(k) == 1:
                bare = sum(1 for c in uses if not c)
                report.setdefault(n, []).append((name, bare, len(uses) - bare))
    for n, rows in sorted(report.items(), key=lambda kv: -len(kv[1])):
        off, size, signed = fields[n]
        print(f'{n} (+{off}, {size} byte{"s" if size > 1 else ""}, port {"signed" if signed else "unsigned"}): '
              f'original {"zero" if signed else "sign"}-extends it in {len(rows)} functions')
        if a.verbose:
            for fn, bare, cast in rows:
                print(f'    {fn:40s} {bare} uncast, {cast} cast')


if __name__ == '__main__':
    main()

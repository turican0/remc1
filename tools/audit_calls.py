#!/usr/bin/env python3
"""Call-graph audit: compare, per function, the calls made by the original code with the calls in the
port's C body. Missing calls point at stubbed / unfinished / mis-decompiled code; unknown call targets
are original functions the port has no counterpart for.

Usage (repo root): python tools/audit_calls.py [--exe <data>/CD/CARPET.EXE] [--all] > audit_calls.txt
"""
import argparse
import os
import remc1_paths  # noqa: E402
import sys

sys.path.insert(0, os.path.dirname(__file__))
from orig_code import OrigCode
from port_index import PortIndex


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=remc1_paths.original_exe())
    ap.add_argument('--src', default='sub_main.cpp')
    ap.add_argument('--all', action='store_true', help='also list functions without differences')
    a = ap.parse_args()

    idx = PortIndex(a.src)
    oc = OrigCode(a.exe)
    # every port function address plus every direct call target is a function start
    oc.known_starts = set(idx.by_addr)
    for addr in list(idx.by_addr):
        if oc.in_code(addr):
            oc.known_starts |= oc.walk(addr, limit=4000)['calls']

    name_of = {}
    for addr, names in idx.by_addr.items():
        name_of[addr] = names[0]

    results = []
    unknown_targets = {}
    for name, f in sorted(idx.funcs.items(), key=lambda kv: kv[1]['addr'] or 0):
        addr = f['addr']
        if addr is None or not oc.in_code(addr):
            continue
        w = oc.walk(addr)
        asm_calls = w['calls'] | w['tail']
        port_calls = {idx.funcs[c]['addr'] for c in idx.calls(name) if idx.funcs[c]['addr'] is not None}
        missing = sorted(c for c in asm_calls if c not in port_calls and c in name_of)
        unknown = sorted(c for c in asm_calls if c not in name_of)
        extra = sorted(c for c in port_calls if c not in asm_calls and oc.in_code(c))
        for u in unknown:
            unknown_targets.setdefault(u, []).append(name)
        if missing or extra or a.all:
            results.append((name, addr, len(w['insns']), missing, extra, unknown))

    print(f'# {len(idx.funcs)} port functions; {len(results)} with call differences')
    print('# missing = the original calls it, the port does not; extra = the port calls it, the original does not')
    for name, addr, n, missing, extra, unknown in results:
        line = f'{name} @{addr:05X} ({n} insns)'
        if missing:
            line += ' MISSING: ' + ', '.join(f'{name_of[c]}' for c in missing)
        if extra:
            line += ' EXTRA: ' + ', '.join(f'{name_of.get(c, hex(c))}' for c in extra)
        print(line)
    print(f'\n# {len(unknown_targets)} original call targets with no port function (runtime library etc.)')
    for t, users in sorted(unknown_targets.items()):
        print(f'{t:05X}: called by {len(users)}: ' + ', '.join(users[:6]) + (' ...' if len(users) > 6 else ''))


if __name__ == '__main__':
    main()

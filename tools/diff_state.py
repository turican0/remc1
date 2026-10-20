#!/usr/bin/env python3
"""Compare two normalised game-state dumps (remc1 --state-dump <frame>:<file>), e.g. from a 32-bit and a
64-bit build at the first frame where tools/regress.py reports a state difference.

    python tools/diff_state.py a.bin b.bin [--max 40]

A dump is the game state (str_AE400_AE3F0, 232713 bytes), str_AE408_AE3F8 and the map arrays (terrain type,
height, shading, angle/flags: 64 KB each; entity index per tile: 128 KB), with pointer slots already replaced
by address-independent values, so every differing byte is a real difference.
Differences are listed per entity (index, class/model/state, field offset) or per struct offset.
"""
import argparse
import struct

STATE_SIZE = 232713
ENTITY_BASE, ENTITY_SIZE, ENTITY_COUNT = 29795, 164, 1000
MAPS = [('mapTerrainType', 0x10000), ('mapHeightmap', 0x10000), ('mapShading', 0x10000), ('mapAngle', 0x10000),
        ('mapEntityIndex', 0x20000)]
MAPS_SIZE = sum(n for _, n in MAPS)


def where(off, total):
    if off < STATE_SIZE:
        if ENTITY_BASE <= off < ENTITY_BASE + ENTITY_SIZE * ENTITY_COUNT:
            i, f = divmod(off - ENTITY_BASE, ENTITY_SIZE)
            return f'entity {i}', f
        return 'state', off
    ae408 = total - STATE_SIZE - MAPS_SIZE
    if off < STATE_SIZE + ae408:
        return 'AE408', off - STATE_SIZE
    off -= STATE_SIZE + ae408
    for name, n in MAPS:
        if off < n:
            return name, off   # tile = off (bytes) or off // 2 for mapEntityIndex
        off -= n
    return 'beyond', off


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--max', type=int, default=40, help='groups to print')
    a = ap.parse_args()
    x, y = open(a.a, 'rb').read(), open(a.b, 'rb').read()
    if len(x) != len(y):
        print(f'sizes differ: {len(x)} vs {len(y)} (different struct layouts?)')
    groups = {}
    for off in range(min(len(x), len(y))):
        if x[off] != y[off]:
            g, f = where(off, len(x))
            groups.setdefault(g, []).append((f, x[off], y[off]))
    if not groups:
        print('identical')
        return
    print(f'{sum(len(v) for v in groups.values())} bytes differ in {len(groups)} places')
    for g, diffs in list(groups.items())[:a.max]:
        head = ''
        if g.startswith('entity'):
            base = ENTITY_BASE + ENTITY_SIZE * int(g.split()[1])
            cls, model, state = x[base + 64], x[base + 65], x[base + 70]
            head = f' (class {cls} model {model} state {state} in a)'
        print(f'{g}{head}: ' + ', '.join(f'+{f}: {p} vs {q}' for f, p, q in diffs[:12]) +
              (' ...' if len(diffs) > 12 else ''))


if __name__ == '__main__':
    main()

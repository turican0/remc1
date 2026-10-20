#!/usr/bin/env python3
"""Per-function differential test: run the ORIGINAL and the PORT version of entity handlers on the same
snapshot memory (tools/emu.py) and compare every byte they write and their return values.

    python tools/diff_funcs.py SNAPDIR                 # all class/state handlers on matching live entities
    python tools/diff_funcs.py SNAPDIR --func sub_19550 [--entity 447]

A mismatch means the port's function (or something it calls) behaves differently from the original
on real game data. Writes to original-addressed globals are compared with the port's global of the
same name; pointers are compared after translating port addresses to original ones.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from emu import Machine
from port_index import addr_from_name

ENTITY_BASE, ENTITY_SIZE, ENTITY_COUNT = 29795, 164, 1000
CLASS_TABLES = {}


def dispatch_tables(m):
    """{(class, state): (ida handler, port symbol name)} from the original class table dword_96902."""
    out = {}
    img = m.img
    for cls in range(14):
        tbl = struct.unpack('<I', img.read(0x96902 + 18 * cls, 4))[0]
        if (0x96902 + 18 * cls) not in img.fixup_sites:
            continue
        a = tbl
        while True:
            tag, idx, fn, en, x = struct.unpack('<IHIHH', img.read(a, 14))
            if tag == 0 and idx == 0 and fn == 0:
                break
            if fn and (a + 6) in img.fixup_sites:
                out[(cls, idx)] = fn
            a += 14
            if a - tbl > 14 * 200:
                break
    return out


def port_name_for(m, ida):
    best = None
    for s in m.ps.syms:
        if s.tag == 5 and addr_from_name(s.name) == ida:
            if best is None or len(s.name) < len(best):
                best = s.name
    return best


def compare(m, w_orig, ret_orig, w_port, ret_port, show=8):
    """Return a list of human readable differences."""
    diffs = []
    # map original writes to the address the port uses for the same byte
    norm_o = {}
    skip_i = getattr(m, 'skip_ida', [])
    skip_p = getattr(m, 'skip_port', [])
    w_port = {a: v for a, v in w_port.items() if not any(lo <= a < hi for lo, hi in skip_p)}
    for a, v in w_orig.items():
        if any(lo <= a < hi for lo, hi in skip_i):
            continue
        if 0x10000 <= a < 0x150000:
            pa, name = m.ida_to_port(a)
            if pa is None:
                norm_o[('ida', a)] = v
                continue
            norm_o[pa] = v
        else:
            norm_o[a] = v
    keys = set(norm_o) | set(w_port)
    bad = []
    for k in sorted(keys, key=lambda x: x if isinstance(x, int) else x[1]):
        if isinstance(k, tuple):
            bad.append((k, norm_o[k], None))
            continue
        o = norm_o.get(k)
        p = w_port.get(k)
        if o is None:
            o = m.mu.mem_read(k, 1)[0]  # unchanged in the original run = current (restored) value
        if p is None:
            p = m.mu.mem_read(k, 1)[0]
        if o != p and k not in m.port_only:
            bad.append((k, o, p))
    # forgive pointer translations: a dword where port value translates to the original value
    real = []
    skip = set()
    for k, o, p in bad:
        if k in skip or isinstance(k, tuple):
            if isinstance(k, tuple):
                real.append((k, o, p))
            continue
        ok = False
        for start in range(k - 3, k + 1):
            ob = bytes((norm_o.get(start + i, m.mu.mem_read(start + i, 1)[0])) for i in range(4))
            pb = bytes((w_port.get(start + i, m.mu.mem_read(start + i, 1)[0])) for i in range(4))
            ov, pv = struct.unpack('<I', ob)[0], struct.unpack('<I', pb)[0]
            if ov != pv and m.translate_value(pv) == ov:
                ok = True
                skip.update(range(start, start + 4))
                break
        if not ok:
            real.append((k, o, p))
    ret_note = f'return value: original {ret_orig:#x}, port {ret_port:#x}' if ret_orig != ret_port else None
    # describe
    st, ss = m.state_addr, m.state_size
    runs = []
    for k, o, p in real:
        if isinstance(k, tuple):
            runs.append(f'original wrote unmapped original address {k[1]:#x}')
            continue
        if st <= k < st + ss:
            off = k - st
            if ENTITY_BASE <= off < ENTITY_BASE + ENTITY_SIZE * ENTITY_COUNT:
                e, f = divmod(off - ENTITY_BASE, ENTITY_SIZE)
                runs.append(f'entity {e} +{f}: original {o} port {p}')
            else:
                runs.append(f'state +{off}: original {o} port {p}')
        else:
            name = None
            for ida, size, pa, nm in m.global_map:
                if pa <= k < pa + size:
                    name = f'{nm}+{k - pa}'
                    break
            runs.append(f'{name or hex(k)}: original {o} port {p}')
    diffs += runs[:show]
    if len(runs) > show:
        diffs.append(f'... {len(runs) - show} more bytes')
    if diffs and ret_note:
        diffs.append(ret_note)
    return diffs, ret_note


def run_pair(m, ida, pname, args):
    m.begin()
    r_o = m.call(ida, args)
    err_o = m.stop_reason
    w_o = m.end()
    m.undo()
    m.begin()
    r_p = m.call(m.port_addr(pname), args)
    err_p = m.stop_reason
    w_p = m.end()
    m.undo()
    return r_o, err_o, w_o, r_p, err_p, w_p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snapdir')
    ap.add_argument('--func', help='port function name (defaults: every class/state handler)')
    ap.add_argument('--entity', type=int, action='append', help='entity index argument (repeatable)')
    ap.add_argument('--max-per-handler', type=int, default=6)
    ap.add_argument('--show', type=int, default=8)
    a = ap.parse_args()
    m = Machine(a.snapdir)
    st = m.state_addr
    ent = lambda i: st + ENTITY_BASE + ENTITY_SIZE * i
    state = bytes(m.mu.mem_read(st, m.state_size))

    jobs = []  # (label, ida, port name, args)
    if a.func:
        ida = addr_from_name(a.func)
        for e in a.entity or []:
            jobs.append((f'{a.func}(entity {e})', ida, a.func, [ent(e)]))
    else:
        tables = dispatch_tables(m)
        by_handler = {}
        for i in range(1, ENTITY_COUNT):
            eo = ENTITY_BASE + ENTITY_SIZE * i
            cls, stt = state[eo + 64], state[eo + 70]
            if not cls:
                continue
            fn = tables.get((cls, stt))
            if fn is None:
                continue
            by_handler.setdefault(fn, []).append(i)
        for fn, ents in sorted(by_handler.items()):
            pname = port_name_for(m, fn)
            if pname is None:
                print(f'handler {fn:05X}: no port function')
                continue
            for e in ents[:a.max_per_handler]:
                eo = ENTITY_BASE + ENTITY_SIZE * e
                jobs.append((f'{pname}(entity {e}, class {state[eo + 64]} model {state[eo + 65]} state {state[eo + 70]})',
                             fn, pname, [ent(e)]))
    nbad = 0
    for label, ida, pname, args in jobs:
        r_o, err_o, w_o, r_p, err_p, w_p = run_pair(m, ida, pname, args)
        if err_o or err_p:
            print(f'{label}: ERROR original: {err_o}  port: {err_p}')
            nbad += 1
            continue
        d, ret_note = compare(m, w_o, r_o, w_p, r_p, a.show)
        if not d and ret_note:
            print(f'{label}: same memory ({len(w_o)} bytes written), {ret_note} (void functions: ignore)')
            continue
        if d:
            nbad += 1
            print(f'{label}: DIFFERS ({len(w_o)} / {len(w_p)} bytes written)')
            for x in d:
                print('    ' + x)
        else:
            print(f'{label}: same ({len(w_o)} bytes written)')
    print(f'# {len(jobs)} calls, {nbad} differ or failed')


if __name__ == '__main__':
    main()

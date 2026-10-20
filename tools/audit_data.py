#!/usr/bin/env python3
"""Data audit: compare the initial contents of every port global named after an original data address
(dword_999B8, str_9688C, creatureTypes_98F38, str_2563D8 = DOSBox address, ...) with CARPET.EXE.

* plain bytes are compared directly;
* pointer dwords are compared by target: the original's LE fixup target vs. the IDA address of the
  port symbol the port pointer refers to (function names carry their address, data symbols too).
  This checks the dispatch tables (entity state handlers, constructors, spells) entry by entry.

Usage (repo root, after a build): python tools/audit_data.py [--exe ...] [--port build/.../remc1.exe]
"""
import argparse
import bisect
import os
import remc1_paths  # noqa: E402
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from le_image import LeImage
from port_symbols import PortSymbols, SymTagFunction
from port_index import addr_from_name

ADDR_RE = re.compile(r'(?<![0-9A-Fa-f])([0-9A-F]{5,6})(?![0-9A-Fa-f])')
DOSBOX_DATA = 0x1BE000   # str_2563D8 (DOSBox linear) = IDA 0x983D8
DOSBOX_CODE = 0x1D1000   # //2126B0_ = IDA 0x416B0


def name_addresses(name, img, code=False):
    out = []
    for m in ADDR_RE.finditer(name):
        a = int(m.group(1), 16)
        for cand in (a, a - DOSBOX_DATA, a - DOSBOX_CODE):
            base = img.object_at(cand) if cand > 0 else None
            if base is None:
                continue
            is_code = base == img.objects[0][0]
            if is_code == code:
                out.append(cand)
                break
    return out


class PortAddrMap:
    """port RVA -> (symbol, IDA address of that byte) using symbol names."""

    def __init__(self, ps, img):
        self.ps = ps
        syms = [s for s in ps.syms if s.rva > 0]
        syms.sort(key=lambda s: s.rva)
        self.syms = syms
        self.rvas = [s.rva for s in syms]
        self.img = img

    def lookup(self, rva):
        # incremental linking: function pointers go to a "jmp target" thunk
        for _ in range(3):
            b = self.ps.initial_bytes(rva, 5)
            if b[0] != 0xE9:
                break
            i = bisect.bisect_right(self.rvas, rva) - 1
            if i >= 0 and self.syms[i].rva == rva and self.syms[i].tag == SymTagFunction:
                break
            rva = rva + 5 + struct.unpack('<i', b[1:5])[0]
        i = bisect.bisect_right(self.rvas, rva) - 1
        if i < 0:
            return None, None
        s = self.syms[i]
        off = rva - s.rva
        if s.tag == SymTagFunction:
            if off:
                return s, None
            # 64-bit builds: PORT_FN thunks (engine/ptr32.h) stand in for the function they convert for
            tname = re.search(r',&(\w+)>::call$', s.name) if s.name.startswith('PortThunkImpl<') else None
            a = addr_from_name(tname.group(1) if tname else s.name)
            if a is None or not (self.img.objects[0][0] <= a < self.img.objects[0][0] + self.img.objects[0][1]):
                return s, None
            return s, a
        if off >= max(s.size, 1):
            return s, None
        cands = name_addresses(s.name, self.img)
        return s, (cands[0] + off) if cands else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=remc1_paths.original_exe())
    ap.add_argument('--port', help='built port executable (default: newest build)')
    ap.add_argument('--show', type=int, default=6, help='differing items shown per symbol')
    a = ap.parse_args()
    img = LeImage(a.exe)
    mem = img.memory()
    ps = PortSymbols(a.port or remc1_paths.find_port_exe())
    amap = PortAddrMap(ps, img)
    orig_ptr = {}
    for site, (t, tgt) in img.fixup_sites.items():
        if t == 7:
            orig_ptr[site] = tgt
    orig_ptr_bytes = set()
    for site in orig_ptr:
        orig_ptr_bytes.update(range(site, site + 4))

    rows = []
    for s in ps.data_symbols():
        cands = name_addresses(s.name, img)
        if not cands or s.size == 0:
            continue
        # the global in the original's 32-bit layout, its pointers as game offsets (statics at ps.STATICS + RVA)
        pb, _, poffs = ps.project32(s, ps.initial_bytes(s.rva, s.size), lambda v: v - ps.image_base + ps.STATICS)
        port_ptr = {o: struct.unpack_from('<I', pb, o)[0] for o in poffs if o + 4 <= len(pb)}
        port_ptr = {o: v for o, v in port_ptr.items() if v}
        port_ptr_bytes = set()
        for o in port_ptr:
            port_ptr_bytes.update(range(o, o + 4))
        best = None
        for addr in cands:
            base = img.object_at(addr)
            buf = mem[base]
            o = addr - base
            n = min(len(pb), len(buf) - o)
            ob = bytes(buf[o:o + n])
            diffs = [i for i in range(n) if i not in port_ptr_bytes and (addr + i) not in orig_ptr_bytes and ob[i] != pb[i]]
            # pointer comparison
            pdiffs = []
            for i in range(0, n - 3):
                op = orig_ptr.get(addr + i)
                pp = port_ptr.get(i)
                if op is None and pp is None:
                    continue
                if op is not None and pp is not None:
                    tsym, tida = amap.lookup(pp - ps.STATICS)
                    if tida is None:
                        pdiffs.append((i, op, f'{tsym.name if tsym else "?"} (no address)'))
                    elif tida != op:
                        pdiffs.append((i, op, f'{tsym.name}+{(pp - ps.STATICS) - tsym.rva} -> {tida:X}'))
                elif op is not None:
                    v = struct.unpack_from('<I', pb, i)[0]
                    if v - DOSBOX_DATA == op:
                        continue  # DOSBox-address constant standing in for the pointer (0x2548F4 type tags)
                    pdiffs.append((i, op, f'not a pointer in port ({v:#x})'))
                else:
                    tsym, tida = amap.lookup(pp - ps.STATICS)
                    ov = struct.unpack_from('<I', ob, i)[0]
                    pdiffs.append((i, None, f'port pointer {tsym.name if tsym else "?"}, original value {ov:#x}'))
            score = len(diffs) + 4 * len(pdiffs)
            if best is None or score < best[0]:
                best = (score, addr, diffs, pdiffs, ob, n)
        score, addr, diffs, pdiffs, ob, n = best
        if diffs or pdiffs:
            rows.append((score, s, addr, diffs, pdiffs, ob, pb, n))

    rows.sort(key=lambda r: -r[0])
    print(f'# {len(rows)} port globals differ from the original data')
    for score, s, addr, diffs, pdiffs, ob, pb, n in rows:
        kind = ''
        if diffs:
            allzero_port = not any(pb[i] for i in diffs)
            allzero_orig = not any(ob[i] for i in diffs)
            kind = 'PORT ZERO' if allzero_port else ('ORIG ZERO' if allzero_orig else 'VALUES')
        print(f'{s.name} @{addr:X} size {s.size} (orig avail {n}): {len(diffs)} bytes differ [{kind}]'
              f'{", %d pointers differ" % len(pdiffs) if pdiffs else ""}')
        shown = 0
        i = 0
        while i < len(diffs) and shown < a.show:
            d = diffs[i] & ~3
            print(f'    +{d:5d}: orig {ob[d:d + 8].hex(" ")}  port {pb[d:d + 8].hex(" ")}')
            while i < len(diffs) and diffs[i] < d + 8:
                i += 1
            shown += 1
        for i, op, what in pdiffs[:a.show]:
            print(f'    ptr +{i}: orig -> {op:X}' if op is not None else f'    ptr +{i}:', what)


if __name__ == '__main__':
    main()

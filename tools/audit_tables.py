#!/usr/bin/env python3
"""Compare the entity dispatch tables (14-byte entries {type tag, index, handler, enabled, ?}) of the port
with the original entry by entry, walking the ORIGINAL table up to its terminator - finds entries the
port lost (truncated initialisers) as well as different handlers.

Usage (repo root, after a build): python tools/audit_tables.py
"""
import os
import remc1_paths  # noqa: E402
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from le_image import LeImage
from port_symbols import PortSymbols
from audit_data import PortAddrMap, name_addresses

TABLES = ['str_254A34', 'str_254D48', 'str_254B84', 'str_254DBC', 'str_255478', 'str_2555A0', 'str_255620',
          'str_2556D8', 'str_255870', 'str_255D0C', 'str_256208', 'str_2567D8', 'str_256980',
          'str_254A08', 'str_254C3C', 'str_254ADC', 'str_254DAC', 'str_254DCC', 'str_255574', 'str_2555CC',
          'str_255674', 'str_25573C', 'str_255998', 'str_256038', 'str_2563D8', 'str_256938']


def main():
    img = LeImage(remc1_paths.original_exe())
    img.memory()
    ps = PortSymbols(remc1_paths.find_port_exe())
    amap = PortAddrMap(ps, img)
    bn = ps.by_name()
    for t in TABLES:
        s = bn.get(t)
        oaddr = name_addresses(t, img)[0]
        # original entries until a terminator (everything zero)
        orig = []
        a = oaddr
        while True:
            raw = img.read(a, 14)
            tag, idx, fn, en, x = struct.unpack('<IHIHH', raw)
            if tag == 0 and fn == 0 and idx == 0 and en == 0:
                break
            orig.append((idx, fn if (a + 6) in img.fixup_sites else (fn or None), en, x))
            a += 14
            if len(orig) > 300:
                break
        port = []
        if s:
            # the table in the original's layout (8-byte function pointers -> 4-byte game offsets)
            data = ps.project32(s, ps.initial_bytes(s.rva, s.size), lambda v: v - ps.image_base + ps.STATICS)[0]
            for k in range(len(data) // 14):
                raw = data[14 * k:14 * k + 14]
                tag, idx, fn, en, x = struct.unpack('<IHIHH', raw)
                if tag == 0 and fn == 0 and idx == 0 and en == 0:
                    break
                tgt = None
                if fn:
                    sym, ida = amap.lookup(fn - ps.STATICS)
                    tgt = ida if ida is not None else ('?' + (sym.name if sym else ''))
                port.append((idx, tgt, en, x))
        issues = []
        for k in range(max(len(orig), len(port))):
            o = orig[k] if k < len(orig) else None
            p = port[k] if k < len(port) else None
            if o != p:
                fo = f'{o[0]}:{o[1]:05X}/{o[2]}' if o and isinstance(o[1], int) else str(o)
                fp = f'{p[0]}:{p[1]:05X}/{p[2]}' if p and isinstance(p[1], int) else str(p)
                issues.append(f'  [{k}] orig {fo}  port {fp}')
        print(f'{t} @{oaddr:X}: orig {len(orig)} entries, port {len(port)}' + (' OK' if not issues else ''))
        for i in issues[:12]:
            print(i)
        if len(issues) > 12:
            print(f'  ... {len(issues) - 12} more')


if __name__ == '__main__':
    main()

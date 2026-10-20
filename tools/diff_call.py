#!/usr/bin/env python3
"""Differential test of one recorded call (e.g. the world renderer): original vs port code.

1. remc1.exe ... --snapshot-frame N --snapshot-what draw --snapshot-dir D
   (dumps memory around one DrawWorld_30D90 call; meta.txt holds the original address + arguments)
2. python tools/diff_call.py D [--png OUTDIR]

Runs the original function and the port function with the recorded arguments on the same memory,
compares every byte written and (for "draw") saves both resulting screens as PNG.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from emu import Machine
from port_index import addr_from_name
from diff_funcs import compare, port_name_for


def screen_png(m, path, w, h, pitch, base, palette, written):
    """The screen as the call left it: memory before the call plus its writes (on 64-bit builds the port's
    writes happen in the harness' 64-bit machine, so they are taken from the write log, not from m.mu)."""
    from PIL import Image
    data = bytearray(m.mu.mem_read(base, pitch * h))
    for addr, v in written.items():
        if base <= addr < base + pitch * h:
            data[addr - base] = v
    img = Image.frombytes('P', (w, h), b''.join(data[y * pitch:y * pitch + w] for y in range(h)))
    img.putpalette(palette)
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snapdir')
    ap.add_argument('--png', help='directory for original.png / port.png / diff.png')
    ap.add_argument('--show', type=int, default=12)
    a = ap.parse_args()
    m = Machine(a.snapdir)
    call = m.meta['call']
    ida, args = call[0], call[1:]
    pname = port_name_for(m, ida)
    print(f'call {pname} @{ida:X} args {args}')

    def g(name, fmt):
        return struct.unpack(fmt, bytes(m.mu.mem_read(m.port_addr(name), struct.calcsize(fmt))))[0]

    res = {}
    # extra leading arguments of the original that the port reads from globals instead
    orig_args = list(args)
    if ida == 0x30D90:  # DrawWorld: the original's first argument is the screen buffer (push [12EFF4h])
        orig_args = [struct.unpack('<I', bytes(m.mu.mem_read(0x12EFF4, 4)))[0]] + orig_args
    for label, addr, ar in (('original', ida, orig_args), ('port', m.port_addr(pname), args)):
        m.begin()
        r = m.call(addr, ar, max_insns=2_000_000_000)
        err = m.stop_reason
        w = m.end()
        if a.png and not err:
            os.makedirs(a.png, exist_ok=True)
            base = g('beginFrame_93ACC', '<I')
            pitch = g('pitchViewPort_93AD4', '<I')
            wv, hv = g('widthViewPort_93AD8', '<H'), g('heightViewPort_93ADC', '<H')
            pal = bytes(m.mu.mem_read(m.port_addr('m_currentPalletColours') if 'm_currentPalletColours' in m.sym_by_name else 0, 0)) or None
            gray = [v for i in range(256) for v in (i, i, i)]
            screen_png(m, os.path.join(a.png, f'{label}.png'), wv, hv, pitch, base, gray, w)
        m.undo()
        res[label] = (r, err, w)
        print(f'{label}: {"ERROR " + err if err else "ok"}, {len(w)} bytes written')
    (r_o, e_o, w_o), (r_p, e_p, w_p) = res['original'], res['port']
    if e_o or e_p:
        return 2
    d, _ = compare(m, w_o, r_o, w_p, r_p, a.show)
    print('identical' if not d else 'DIFFERS:')
    for x in d:
        print('   ', x)
    if a.png:
        from PIL import Image, ImageChops
        o = Image.open(os.path.join(a.png, 'original.png')).convert('RGB')
        p = Image.open(os.path.join(a.png, 'port.png')).convert('RGB')
        diff = ImageChops.difference(o, p)
        print('screen difference bbox:', diff.getbbox())
        diff.point(lambda v: 255 if v else 0).save(os.path.join(a.png, 'diff.png'))
    return 0


if __name__ == '__main__':
    sys.exit(main())

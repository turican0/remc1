#!/usr/bin/env python3
"""Disassemble original functions (relocated, control-flow ordered by address).

Usage: python tools/orig_disasm.py 0x40DF0 [0x20F80 ...] [--max 80]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from orig_code import OrigCode


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    mx = 80
    if '--max' in sys.argv:
        mx = int(sys.argv[sys.argv.index('--max') + 1])
        args = [a for a in args if a != str(mx)]
    oc = OrigCode()
    for a in args:
        addr = int(a, 16)
        w = oc.walk(addr)
        ins = [w['insns'][k] for k in sorted(w['insns'])]
        print(f'== {addr:05X}: {len(ins)} insns, calls ' + ', '.join(f'{c:05X}' for c in sorted(w['calls'])))
        for i in ins[:mx]:
            print(f'  {i.address:05X}: {i.mnemonic} {i.op_str}')
        if len(ins) > mx:
            print(f'  ... {len(ins) - mx} more')


if __name__ == '__main__':
    main()

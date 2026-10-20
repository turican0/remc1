#!/usr/bin/env python3
"""Constant/offset audit: per function, numbers the original code uses (immediates, struct displacements)
that do not appear anywhere in the port's C body - neither as a literal nor as the offset suffix of a
field name (var_u16_29821_26 -> 26 / 29821, collisionBox_29873_78 -> 78 ...).

Missing struct offsets point at fields the port reads/writes elsewhere (wrong offset, typed-pointer
arithmetic, missing statements); missing immediates at wrong constants, table indices, limits.
Noise (compiler magic, stack offsets, tiny numbers) is filtered; results are ranked.

Usage (repo root): python tools/audit_consts.py [--min 2] [--func NAME]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from capstone import x86
from orig_code import OrigCode
from port_index import PortIndex

NUM_RE = re.compile(r'\b(0x[0-9A-Fa-f]+|\d+)(?:[uUlL]|i64|ui64)*\b')
IDENT_RE = re.compile(r'\b[A-Za-z_]\w*\b')
SUFFIX_RE = re.compile(r'_(\d+)(?=_|$)')
HEXSUFFIX_RE = re.compile(r'_([0-9A-F]{5,6})(?=_|$)')

SMALL = set(range(-8, 9)) | {16, 32, 64, 128, 255, 256, 0xFF, 0xFFFF, 0xFFFFFFFF, -16, -32}
STACK_REGS = {x86.X86_REG_ESP}


def port_numbers(body):
    nums = set()
    for m in NUM_RE.finditer(body):
        t = m.group(1)
        try:
            v = int(t, 16) if t.lower().startswith('0x') else int(t)
        except ValueError:
            continue
        nums.add(v)
        nums.add(-v)
        nums.add(v & 0xFFFFFFFF)
        nums.add((-v) & 0xFFFFFFFF)
    for m in IDENT_RE.finditer(body):
        ident = m.group(0)
        for s in SUFFIX_RE.findall(ident):
            v = int(s)
            nums.add(v)
        for s in HEXSUFFIX_RE.findall(ident):
            nums.add(int(s, 16))
    return nums


def explained(v, nums):
    if v in nums:
        return True
    # entity array element offsets: base 29795 + 164*i + field
    for base in (29795, 0):
        if v > base and (v - base) % 164 in nums and base:
            return True
    # word/dword index scaled
    for sc in (2, 4, 6, 8, 12, 14, 18, 164, 2049):
        if v % sc == 0 and v // sc in nums and v // sc not in SMALL:
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='sub_main.cpp')
    ap.add_argument('--min', type=int, default=1)
    ap.add_argument('--func')
    a = ap.parse_args()
    idx = PortIndex(a.src)
    oc = OrigCode()
    oc.known_starts = set(idx.by_addr)
    fixups = oc.img.fixup_sites
    rows = []
    for name, f in idx.funcs.items():
        if f['addr'] is None or not oc.in_code(f['addr']):
            continue
        if a.func and name != a.func:
            continue
        body = f['body']
        if body.strip() in ('', 'return 0;', 'return;'):
            continue
        nums = port_numbers(body)
        w = oc.walk(f['addr'])
        # functions with a frame pointer (mov ebp, esp) address locals/arguments through ebp
        frame = any(i.mnemonic == 'mov' and i.op_str == 'ebp, esp' for i in w['insns'].values())
        missing_disp, missing_imm = {}, {}
        for ins in w['insns'].values():
            # skip operands that are relocated addresses (globals/code): checked by other audits
            reloc = any(s in fixups for s in range(ins.address, ins.address + ins.size))
            for op in ins.operands:
                if op.type == x86.X86_OP_MEM:
                    if op.mem.base in STACK_REGS or op.mem.base == 0 and op.mem.index == 0:
                        continue
                    if frame and op.mem.base == x86.X86_REG_EBP:
                        continue
                    d = op.mem.disp
                    if reloc and (d & 0xFFFFFFFF) >= 0x10000:
                        continue
                    if d in SMALL or explained(d, nums):
                        continue
                    missing_disp.setdefault(d, []).append(ins.address)
                elif op.type == x86.X86_OP_IMM:
                    v = op.imm
                    if ins.mnemonic in ('call', 'jmp') or ins.mnemonic.startswith('j'):
                        continue
                    if reloc and (v & 0xFFFFFFFF) >= 0x10000:
                        continue
                    if ins.mnemonic in ('add', 'sub') and ins.operands[0].type == x86.X86_OP_REG and \
                            ins.operands[0].reg == x86.X86_REG_ESP:
                        continue
                    sv = v - (1 << 32) if v >= (1 << 31) else v
                    if sv in SMALL or explained(sv, nums) or explained(v & 0xFFFFFFFF, nums):
                        continue
                    if abs(sv) >= 0x10000000:  # division / multiplication magic numbers
                        continue
                    if ins.mnemonic in ('shl', 'shr', 'sar', 'rol', 'ror', 'sal'):
                        continue
                    missing_imm.setdefault(sv, []).append(ins.address)
        score = 3 * len(missing_disp) + len(missing_imm)
        if score >= a.min:
            rows.append((score, name, f, missing_disp, missing_imm, len(w['insns'])))
    rows.sort(key=lambda r: -r[0])
    print(f'# {len(rows)} functions with unexplained constants (score = 3*offsets + immediates)')
    for score, name, f, md, mi, n in rows:
        print(f'{name} @{f["addr"]:05X} line {f["line"]} ({n} insns) score {score}')
        if md:
            print('   offsets: ' + ', '.join(f'{d:#x}@{min(ad):05X}' for d, ad in sorted(md.items())))
        if mi:
            print('   immediates: ' + ', '.join(f'{v}@{min(ad):05X}' for v, ad in sorted(mi.items())))


if __name__ == '__main__':
    main()

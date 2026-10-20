#!/usr/bin/env python3
"""Find call sites where the original code uses a register value that the callee has overwritten.

Watcom code normally preserves every register except eax across a call, and Hex-Rays decompiles callers
on that assumption. A few routines (hand-written asm, #pragma aux helpers) leave scratch values in ecx /
edx / ... without saving them. When a caller then reads such a register right after the call, the
original uses the callee's leftover value while the decompiled code uses the caller's old value.
Example: sub_54F80 pushes edx after calling sub_42410 (which leaves dy*dy in edx) as the argument of
the square root at 0x410CE - Hex-Rays showed the player index there.

For every original function reachable from the port's function addresses this computes the registers
(ebx ecx edx esi edi ebp) written but not saved (push before first write), including through callees,
and lists call sites whose caller reads one of them before writing it, along the straight-line path
after the call.

    python tools/audit_clobber.py [--max-insns 12]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capstone
from capstone import x86
from orig_code import OrigCode
from port_index import PortIndex

TRACKED = ('ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
FULL = {}
for r in TRACKED:
    FULL[r] = r
for full, subs in (('ebx', 'bx bl bh'), ('ecx', 'cx cl ch'), ('edx', 'dx dl dh'), ('esi', 'si sil'),
                   ('edi', 'di dil'), ('ebp', 'bp bpl')):
    for s in subs.split():
        FULL[s] = full


def regs(ins):
    try:
        r, w = ins.regs_access()
    except capstone.CsError:
        return set(), set()
    rd = {FULL[ins.reg_name(x)] for x in r if ins.reg_name(x) in FULL}
    wr = {FULL[ins.reg_name(x)] for x in w if ins.reg_name(x) in FULL}
    m = ins.mnemonic
    # xor r, r / sub r, r define without reading
    if m in ('xor', 'sub') and len(ins.operands) == 2 and all(o.type == x86.X86_OP_REG for o in ins.operands) \
            and ins.operands[0].reg == ins.operands[1].reg:
        rd -= wr
    # a write to a partial register keeps the rest: treat as read+write (conservative for "defined")
    return rd, wr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--max-insns', type=int, default=12)
    a = ap.parse_args()
    oc = OrigCode()
    idx = PortIndex('sub_main.cpp')
    starts = {f['addr'] for f in idx.funcs.values() if f['addr'] and oc.in_code(f['addr'])}
    oc.known_starts = set(starts)
    funcs = {}
    todo = list(starts)
    while todo:
        s = todo.pop()
        if s in funcs or not oc.in_code(s):
            continue
        w = oc.walk(s)
        funcs[s] = w
        for c in w['calls'] | w['tail']:
            if c not in funcs:
                oc.known_starts.add(c)
                todo.append(c)
    # own clobbers: written, and not pushed before the first write (in address order: approximation)
    own, callees = {}, {}
    for s, w in funcs.items():
        saved, written = set(), set()
        for addr in sorted(w['insns']):
            ins = w['insns'][addr]
            if ins.mnemonic in ('pushal', 'pushad') and not written:
                saved |= set(TRACKED)
                continue
            if ins.mnemonic == 'push' and ins.operands[0].type == x86.X86_OP_REG:
                r = FULL.get(ins.reg_name(ins.operands[0].reg))
                if r and r not in written:
                    saved.add(r)
                continue
            if ins.mnemonic in ('pop', 'call', 'leave', 'popal', 'popad', 'ret'):
                continue
            rd, wr = regs(ins)
            written |= wr
        own[s] = written - saved
        callees[s] = (w['calls'] | w['tail'], saved)
    clob = dict(own)
    changed = True
    while changed:
        changed = False
        for s, (cs, saved) in callees.items():
            add = set()
            for c in cs:
                add |= clob.get(c, set())
            add -= saved
            if not add <= clob[s]:
                clob[s] |= add
                changed = True
    names = {}
    for n, f in idx.funcs.items():
        if f['addr']:
            names.setdefault(f['addr'], n)
    hits = 0
    for s, w in sorted(funcs.items()):
        ins_by = w['insns']
        for addr in sorted(ins_by):
            ins = ins_by[addr]
            if ins.mnemonic != 'call' or ins.operands[0].type != x86.X86_OP_IMM:
                continue
            callee = ins.operands[0].imm & 0xFFFFFFFF
            dirty = set(clob.get(callee, set()))
            if not dirty:
                continue
            # paths after the call (both ways at conditional jumps), up to max_insns instructions each
            work = [(addr + ins.size, frozenset(dirty), 0)]
            seen_p = set()
            found = None
            while work and not found:
                p, d, n = work.pop()
                while d and n < a.max_insns and (p, d) not in seen_p:
                    seen_p.add((p, d))
                    nx = ins_by.get(p)
                    if nx is None:
                        break
                    n += 1
                    rd, wr = regs(nx)
                    ops = nx.operands
                    nop = (nx.mnemonic in ('lea', 'mov') and len(ops) == 2 and nx.op_str.split(', ')[0] in
                           nx.op_str.split(', ')[1].strip('[]').split())  # lea edx, [edx] / mov edx, edx
                    bad = set() if nx.mnemonic == 'pop' or nop else rd & d
                    if bad:
                        found = (p, nx, bad)
                        break
                    d = d - wr
                    m = nx.mnemonic
                    if m in ('ret', 'call', 'jmp') and not (m == 'jmp' and ops[0].type == x86.X86_OP_IMM):
                        break
                    if m == 'jmp':
                        p = ops[0].imm & 0xFFFFFFFF
                        continue
                    if m.startswith('j') or m.startswith('loop'):
                        if ops[0].type == x86.X86_OP_IMM:
                            work.append((ops[0].imm & 0xFFFFFFFF, d, n))
                    p += nx.size
            if found:
                p, nx, bad = found
                hits += 1
                print(f'{names.get(s, hex(s)):32s} {addr:#x}: call {names.get(callee, hex(callee))} '
                      f'leaves {",".join(sorted(bad))}; read at {p:#x}: {nx.mnemonic} {nx.op_str}')
    print(f'# {hits} call sites; {sum(1 for c in clob.values() if c)} of {len(funcs)} functions clobber a saved register')


if __name__ == '__main__':
    main()

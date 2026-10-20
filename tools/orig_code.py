#!/usr/bin/env python3
"""Recursive-descent disassembly of original CARPET.EXE functions (after LE fixups).

walk(addr) follows the control flow of the function starting at `addr`: conditional and unconditional
jumps, Watcom jump tables (jmp cs:[reg*4 + table]) and fall-through, stopping at ret/retn and at jumps
into other known function starts (tail calls). It returns the instructions, the call targets, and the
immediates / memory displacements used (for the constant audit).
"""
import struct
import os
import remc1_paths  # noqa: E402
import capstone
from capstone import x86

from le_image import LeImage


class OrigCode:
    def __init__(self, exe=None):
        exe = exe or remc1_paths.original_exe()
        self.img = LeImage(exe)
        self.mem = self.img.memory()
        self.cbase, self.cend = self.img.code_range()
        self.code = self.mem[self.cbase]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.known_starts = set()
        self._cache = {}

    def in_code(self, a):
        return self.cbase <= a < self.cend

    def insn_at(self, a):
        if a in self._cache:
            return self._cache[a]
        o = a - self.cbase
        ins = next(self.md.disasm(bytes(self.code[o:o + 16]), a, 1), None)
        self._cache[a] = ins
        return ins

    def jump_table(self, ins):
        """Targets of jmp dword ptr [reg*4 + table] (Watcom switch)."""
        if not ins.operands:
            return []
        op = ins.operands[0]
        if op.type != x86.X86_OP_MEM or op.mem.scale != 4 or op.mem.base != 0:
            return []
        table = op.mem.disp & 0xFFFFFFFF
        out = []
        for k in range(512):
            ta = table + 4 * k
            if ta not in self.img.fixup_sites:
                break
            t = struct.unpack_from('<I', self.code, ta - self.cbase)[0] if self.in_code(ta) else None
            if t is None or not self.in_code(t):
                break
            out.append(t)
        return out

    def walk(self, start, limit=20000):
        seen = {}
        calls = set()
        icalls = 0
        todo = [start]
        tail = set()
        while todo and len(seen) < limit:
            a = todo.pop()
            while a not in seen and self.in_code(a):
                ins = self.insn_at(a)
                if ins is None:
                    break
                seen[a] = ins
                m = ins.mnemonic
                nxt = a + ins.size
                if m == 'call':
                    op = ins.operands[0]
                    if op.type == x86.X86_OP_IMM:
                        calls.add(op.imm & 0xFFFFFFFF)
                    else:
                        icalls += 1
                    a = nxt
                    continue
                if m in ('ret', 'retn', 'retf', 'iret', 'iretd', 'hlt'):
                    break
                if m == 'jmp':
                    op = ins.operands[0]
                    if op.type == x86.X86_OP_IMM:
                        t = op.imm & 0xFFFFFFFF
                        if t != start and t in self.known_starts:
                            tail.add(t)
                            break
                        a = t
                        continue
                    for t in self.jump_table(ins):
                        todo.append(t)
                    break
                if m.startswith('j') or m.startswith('loop') or m == 'jecxz':
                    op = ins.operands[0]
                    if op.type == x86.X86_OP_IMM:
                        t = op.imm & 0xFFFFFFFF
                        if t != start and t in self.known_starts:
                            tail.add(t)  # conditional tail call
                        else:
                            todo.append(t)
                a = nxt
        return dict(insns=seen, calls=calls, tail=tail, icalls=icalls)

    @staticmethod
    def constants(insns):
        """Immediates and memory displacements (as ints) used by the instructions."""
        imms, disps = [], []
        for ins in insns.values():
            for op in ins.operands:
                if op.type == x86.X86_OP_IMM:
                    imms.append(op.imm)
                elif op.type == x86.X86_OP_MEM:
                    disps.append(op.mem.disp)
        return imms, disps

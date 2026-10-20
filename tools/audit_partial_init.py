#!/usr/bin/env python3
"""Locals that are read before all their bytes were written (decompilation pattern: Hex-Rays splits one register
into two variables, e.g. `v2` and `result` both `ax`, and the second is only written byte-wise with
LOBYTE/HIBYTE/BYTE1/LOWORD/HIWORD). The read then picks up whatever the compiler left in the other bytes:
right by register luck in one build, garbage in another (sub_33AE0_33ED0 put terrain type 8 on random tiles
in the 64-bit build).

Straight-line scan of each function body in source order (control flow is ignored), so the output is a list of
candidates to check against the original (tools/orig_disasm.py).

Usage (repo root): python tools/audit_partial_init.py [file] [--all]   (default sub_main.cpp)
"""
import re
import sys

DECL = re.compile(r'^\s+((?:unsigned |signed )?(?:__int8|__int16|__int32|int|char|short|_BYTE|_WORD|_DWORD|uint8|uint16|uint32|'
                  r'int8|int16|int32|uint8_t|uint16_t|uint32_t|int8_t|int16_t|int32_t))\s+(\w+)\s*;')
SIZE = {'__int8': 1, 'char': 1, '_BYTE': 1, 'uint8': 1, 'int8': 1, 'uint8_t': 1, 'int8_t': 1,
        '__int16': 2, 'short': 2, '_WORD': 2, 'uint16': 2, 'int16': 2, 'uint16_t': 2, 'int16_t': 2}
FUNC = re.compile(r'^[A-Za-z_][\w\s\*:<>,]*?\b(\w+)\s*\([^;]*\)\s*(//.*)?$')


def parts(macro, size):
    full = set(range(size))
    if macro == 'LOBYTE':
        return {0}
    if macro == 'HIBYTE':
        return {size - 1}
    if macro.startswith('BYTE') and macro[4:].isdigit():
        return {int(macro[4:])} & full
    if macro == 'LOWORD':
        return {0, 1} & full
    if macro == 'HIWORD':
        return {size - 2, size - 1} & full
    return full


def scan(lines):
    out = []
    i = 0
    while i < len(lines):
        m = FUNC.match(lines[i])
        if not (m and i + 1 < len(lines) and lines[i + 1].strip() == '{'):
            i += 1
            continue
        name, j, depth, body = m.group(1), i + 1, 0, []
        while j < len(lines):
            depth += lines[j].count('{') - lines[j].count('}')
            body.append((j + 1, lines[j]))
            j += 1
            if depth == 0:
                break
        sizes = {}
        for n, l in body:
            d = DECL.match(l)
            if d:
                sizes[d.group(2)] = SIZE.get(d.group(1).split()[-1], 4)
        written = {v: set() for v in sizes}
        flagged = set()
        for n, l in body:
            if DECL.match(l) or l.lstrip().startswith('//'):
                continue
            code = l.split('//')[0]
            for v in sizes:
                if v in flagged or v not in code:
                    continue
                for m2 in re.finditer(r'\b(LOBYTE|HIBYTE|BYTE\d|LOWORD|HIWORD)\(\s*' + v + r'\s*\)\s*=(?!=)', code):
                    written[v] |= parts(m2.group(1), sizes[v])
                if re.search(r'(?<![\w.>])' + v + r'\s*=(?!=)', code) and not re.search(r'\(\s*' + v + r'\s*\)\s*=(?!=)', code):
                    rhs_read = re.search(r'=.*\b' + v + r'\b', code.split(v, 1)[1]) is not None
                    if not rhs_read:
                        written[v] = set(range(sizes[v]))
                        continue
                # a read: the variable outside a partial-write target
                stripped = re.sub(r'\b(LOBYTE|HIBYTE|BYTE\d|LOWORD|HIWORD)\(\s*' + v + r'\s*\)\s*=(?!=)', '', code)
                stripped = re.sub(r'(?<![\w.>])' + v + r'\s*=(?!=)', '', stripped)
                if re.search(r'(?<![\w.>])' + v + r'\b', stripped) and written[v] and written[v] != set(range(sizes[v])):
                    reads_part = re.search(r'\b(LOBYTE|HIBYTE|BYTE\d|LOWORD|HIWORD)\(\s*' + v + r'\s*\)', stripped)
                    if not reads_part:
                        out.append((n, name, v, sorted(written[v]), sizes[v], l.strip()[:110]))
                        flagged.add(v)
        i = j
    return out


def main():
    path = next((a for a in sys.argv[1:] if not a.startswith('--')), 'sub_main.cpp')
    lines = open(path, 'rb').read().decode('latin-1').split('\n')
    hits = scan(lines)
    for n, f, v, w, size, l in hits:
        print(f'{path}:{n} {f}: {v} ({size} bytes) read with only bytes {w} written: {l}')
    print(f'{len(hits)} candidates')


if __name__ == '__main__':
    main()

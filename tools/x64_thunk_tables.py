#!/usr/bin/env python3
"""64-bit conversion helper: wraps function-pointer casts in sub_main.cpp in PORT_FN (engine/ptr32.h)
where the function takes or returns an int "address" but the cast signature uses a pointer (or the
other way round), e.g.
    (void (*)(Type_AE400_29795*))sub_253E0      sub_253E0(int a1)
 -> PORT_FN(sub_253E0, void(Type_AE400_29795*))
Casts whose parameter count differs from the function, or that need no conversion, stay as they are.

Usage (repo root): python tools/x64_thunk_tables.py [--dry-run]
"""
import re
import sys

CAST = re.compile(r'\(\s*([A-Za-z_][\w ]*?\s*\**)\s*\(\s*\*\s*\)\s*\(([^()]*)\)\s*\)\s*&?\s*([A-Za-z_]\w*)\b(?!\s*\()')
PROTO = re.compile(r'^\s*(?:static\s+)?([A-Za-z_][\w \*]*?)\s*\b(\w+)\s*\(([^()]*)\)\s*;', re.M)


def params(text):
    text = text.strip()
    if text in ('', 'void'):
        return []
    return [p.strip() for p in text.split(',')]


def kind(t):
    """'ptr', 'int' or 'other' for a parameter/return type (names after the type are ignored)."""
    if '*' in t:
        return 'ptr'
    t = re.sub(r'\b[a-z]\w*$', '', t.strip()) if len(t.split()) > 1 else t
    if re.search(r'\b(void)\b', t):
        return 'void'
    if re.search(r'\b(int|__int8|__int16|__int32|char|short|long|_BYTE|_WORD|_DWORD|x_DWORD|uint8|uint16|uint32|int8|int16|int32|'
                 r'uint8_t|uint16_t|uint32_t|int8_t|int16_t|int32_t|unsigned|signed|_BOOL1|bool)\b', t):
        return 'int'
    return 'other'


def main():
    dry = '--dry-run' in sys.argv
    src = open('sub_main.cpp', 'rb').read().decode('latin-1')
    protos = {}
    for text in (src, open('sub_main.h', 'rb').read().decode('latin-1')):
        for m in PROTO.finditer(text):
            protos.setdefault(m.group(2), (m.group(1), params(m.group(3))))
    done, skipped = 0, {}

    def repl(m):
        nonlocal done
        cret, cpar, name = m.group(1).strip(), params(m.group(2)), m.group(3)
        if name not in protos:
            skipped['no prototype'] = skipped.get('no prototype', 0) + 1
            return m.group(0)
        fret, fpar = protos[name]
        if len(fpar) != len(cpar):
            skipped['parameter count'] = skipped.get('parameter count', 0) + 1
            return m.group(0)
        pairs = [(kind(c), kind(f)) for c, f in zip(cpar + [cret], fpar + [fret])]
        if not any({a, b} == {'ptr', 'int'} for a, b in pairs):
            return m.group(0)
        done += 1
        return f'PORT_FN({name}, {cret}({", ".join(cpar)}))'

    out = CAST.sub(repl, src)
    print(f'{done} casts wrapped in PORT_FN; left alone: {skipped}')
    if not dry:
        open('sub_main.cpp', 'wb').write(out.encode('latin-1'))


if __name__ == '__main__':
    main()

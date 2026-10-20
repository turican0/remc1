#!/usr/bin/env python3
"""Find Hex-Rays "byte offset added to a typed pointer" bugs in sub_main.cpp.

IDA emitted e.g. `*(_BYTE *)(ptr + 392)` with `ptr` as a 32-bit int. When `ptr` later became a typed
pointer (Type_160*, TColor*, uint16_t*, ...) the expression silently started scaling by
sizeof(*ptr). This lists dereferences `*(<cast> *)(... NAME + ...)` where NAME is
  * a global / struct field declared with a non-byte pointee type, or
  * a parameter or local of the enclosing function declared that way.
Review each hit by hand, then fix with an explicit `(uint8_t*)NAME` cast.

Usage: python tools/scan_ptr_arith.py [--fix]   (--fix applies the cast to every reported line)
"""
import re
import sys

SRC = 'sub_main.cpp'
HDRS = ['sub_main.h', 'engine/Basic.h']
BYTE_TYPES = {'uint8_t', 'char', 'uint8', 'int8_t', 'int8', '_BYTE', 'void', 'unsigned', 'FILE', 'const',
              'signed', '__int8'}
KW = {'else', 'return', 'result', 'case', 'goto', 'do', 'if', 'while', 'for', 'sizeof', 'new', 'delete'}

decl = re.compile(r'^\s*(?:extern\s+|static\s+|const\s+)?(?:struct\s+|unsigned\s+)?(\w+)\s*\*\s*(\w+)\s*(?:=[^;]*)?;', re.M)
deref = re.compile(r'\*\s*\(\s*(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:_BYTE|_WORD|_DWORD|__int8|__int16|__int32|int|uint8_t|uint16_t|uint32_t|int8_t|int16_t|int32_t|char|int16|uint16|uint32|uint8|int32)\s*\*\s*\)\s*\(([^;]*)')
func_start = re.compile(r'^[\w][\w\s\*]*?\b(\w+)\s*\(([^;{}]*)\)\s*(?://.*)?$')


def typed_ptr_names_from(text):
    out = {}
    for t, n in decl.findall(text):
        if t not in BYTE_TYPES and t not in KW and n not in KW and len(n) > 2:
            out[n] = t
    return out


def params(sig):
    out = {}
    for p in sig.split(','):
        m = re.match(r'\s*(?:const\s+)?(?:struct\s+|unsigned\s+)?(\w+)\s*\*\s*(\w+)\s*$', p)
        if m and m.group(1) not in BYTE_TYPES:
            out[m.group(2)] = m.group(1)
    return out


def scan(lines):
    globals_ = typed_ptr_names_from('\n'.join(open(h, encoding='latin-1').read() for h in HDRS))
    # file-scope declarations (no indentation)
    for t, n in re.findall(r'^(?:struct\s+)?(\w+)\s*\*\s*(\w+)\s*(?:=[^;]*)?;', '\n'.join(lines), re.M):
        if t not in BYTE_TYPES and t not in KW and len(n) > 2:
            globals_[n] = t
    hits = []
    scope = {}
    globals_shadow = set()
    depth = 0
    for i, l in enumerate(lines):
        stripped = l.strip()
        if depth == 0:
            m = func_start.match(l)
            if m and ((i + 1 < len(lines) and lines[i + 1].strip().startswith('{')) or stripped.endswith('{')):
                scope = params(m.group(2))
                globals_shadow = set()
                # non-pointer parameters shadow globals too
                for p in m.group(2).split(','):
                    pm = re.match(r'\s*(?:const\s+|unsigned\s+)?\w+\s+(\w+)\s*$', p)
                    if pm:
                        globals_shadow.add(pm.group(1))
        elif depth >= 1:
            lm = re.match(r'^\s+(?:const\s+)?(?:struct\s+)?(\w+)\s*\*\s*(\w+)\s*(?:=[^;]*)?;', l)
            if lm and lm.group(1) not in BYTE_TYPES and lm.group(1) not in KW:
                scope[lm.group(2)] = lm.group(1)
            elif lm:
                scope.pop(lm.group(2), None)
            # plain (non-pointer) local declaration shadows any typed pointer of the same name
            pm = re.match(r'^\s+(?:unsigned\s+|signed\s+|const\s+)?(\w+)\s+(\w+)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;', l)
            if pm and pm.group(1) not in KW and pm.group(1) not in ('return', 'goto'):
                scope.pop(pm.group(2), None)
                globals_shadow.add(pm.group(2))
        depth += l.count('{') - l.count('}')
        if depth <= 0:
            depth = 0
        if stripped.startswith('//'):
            continue
        names = {k: v for k, v in globals_.items() if k not in globals_shadow}
        names.update(scope)
        for m in deref.finditer(l):
            inner = m.group(1)
            for n, t in names.items():
                if n not in inner:
                    continue
                # NAME + x / NAME - <digit> / x + NAME ; not *NAME, ->NAME, NAME - otherptr (index math)
                if re.search(r'(?<![\w>.*])' + n + r'\s*(?:\+\s*[\w(]|-\s*\d)|[+]\s*' + n + r'\b(?!\s*\[)(?!\s*->)', inner) and \
                   not re.search(r'\(\s*(?:uint8_t|char|uint8|_BYTE|int|uint32|uint32_t|unsigned int)\s*\*?\s*\)\s*' + n + r'\b', inner):
                    hits.append((i, n, t))
    return hits


def main():
    lines = open(SRC, 'rb').read().decode('latin-1').split('\n')
    hits = scan(lines)
    for i, n, t in hits:
        print(f'{i + 1}: [{n}: {t}*] {lines[i].strip()[:140]}')
    print(f'{len(hits)} suspicious lines', file=sys.stderr)
    if '--fix' in sys.argv:
        for i, n, t in hits:
            # only bare uses: not &NAME, ->NAME, *NAME, NAME[...], NAME->..., already-cast NAME
            lines[i] = re.sub(r'(?<![\w&>*.)])' + n + r'(?!\w)(?!\s*\[)(?!\s*->)', '((uint8_t*)' + n + ')', lines[i])
        open(SRC, 'wb').write('\n'.join(lines).encode('latin-1'))
        print('applied (uint8_t*) casts', file=sys.stderr)


if __name__ == '__main__':
    main()

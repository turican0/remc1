#!/usr/bin/env python3
"""64-bit conversion helper: rewrites the int<->pointer casts that MSVC x64 flags in sub_main.cpp.

The decompiled code keeps addresses in 32-bit ints. In 64-bit builds those ints are offsets from
g_ptrBase (engine/ptr32.h), so each flagged cast converts explicitly:
    C4312  (T *)expr   int -> pointer   becomes  (T *)PTR(expr)    PTR(x) = g_ptrBase + x (0 -> nullptr)
    C4311  (int)ptr    pointer -> int   becomes  (int)OFF(ptr)     OFF(p) = p - g_ptrBase (nullptr -> 0)
On 32-bit builds g_ptrBase is 0, so both are no-ops.

Usage: build win-x64 into a log, then
    python tools/x64_rewrite_casts.py <build log> [--dry-run]
MSVC columns are 1-based with a tab counted as one column and point at the cast's '('. Lines are
edited right to left so earlier columns stay valid; run again after rebuilding until no warnings are left.
"""
import re
import sys

PAT = re.compile(r'sub_main\.cpp\((\d+),(\d+)\): warning (C4312|C4311)')
IDENT = re.compile(r'[A-Za-z_]\w*')


def match_paren(s, i):
    """s[i] == '(' or '[' -> index after the matching closer."""
    open_c = s[i]
    close_c = ')' if open_c == '(' else ']'
    depth = 0
    for j in range(i, len(s)):
        if s[j] == open_c:
            depth += 1
        elif s[j] == close_c:
            depth -= 1
            if depth == 0:
                return j + 1
    raise ValueError('unbalanced')


def skip_ws(s, i):
    while i < len(s) and s[i] in ' \t':
        i += 1
    return i


def looks_like_type(text):
    return re.fullmatch(r'[\w\s\*]+', text) is not None and not re.fullmatch(r'\s*\d+\s*', text)


def parse_postfix(s, i):
    while True:
        j = skip_ws(s, i)
        if s.startswith('->', j):
            m = IDENT.match(s, skip_ws(s, j + 2))
            i = m.end()
        elif j < len(s) and s[j] == '.' and IDENT.match(s, j + 1):
            i = IDENT.match(s, j + 1).end()
        elif j < len(s) and s[j] in '([':
            i = match_paren(s, j)
        elif s.startswith('++', j) or s.startswith('--', j):
            i = j + 2
        else:
            return i


def parse_unary(s, i):
    """End index of the unary-expression starting at s[i] (the operand of a cast)."""
    i = skip_ws(s, i)
    if s.startswith('++', i) or s.startswith('--', i):
        return parse_unary(s, i + 2)
    if s[i] in '*&-!~+':
        return parse_unary(s, i + 1)
    if s[i] == '(':
        end = match_paren(s, i)
        nxt = skip_ws(s, end)
        if looks_like_type(s[i + 1:end - 1]) and nxt < len(s) and (s[nxt] in '(*&' or IDENT.match(s, nxt)):
            return parse_unary(s, end)          # another cast
        return parse_postfix(s, end)
    m = IDENT.match(s, i)
    if m:
        return parse_postfix(s, m.end())
    m = re.match(r'0[xX][0-9A-Fa-f]+[uUlL]*|\d+[uUlL]*', s[i:])
    if m:
        return i + m.end()
    raise ValueError(f'cannot parse operand at {s[i:i + 30]!r}')


def rewrite_line(s, col, kind):
    i = col - 1
    if s[i] != '(':
        raise ValueError(f'no cast at column {col}: {s[i:i + 30]!r}')
    tend = match_paren(s, i)
    op_start = skip_ws(s, tend)
    op_end = parse_unary(s, op_start)
    op = s[op_start:op_end]
    fn = 'PTR' if kind == 'C4312' else 'OFF'
    if op.startswith(fn + '('):
        return s                                 # already converted
    wrapped = f'{fn}{op}' if op.startswith('(') and match_paren(op, 0) == len(op) else f'{fn}({op})'
    return s[:op_start] + wrapped + s[op_end:]


def main():
    log, dry = sys.argv[1], '--dry-run' in sys.argv
    hits = {}
    for line in open(log, encoding='utf-8', errors='replace'):
        m = PAT.search(line)
        if m:
            hits.setdefault(int(m.group(1)), set()).add((int(m.group(2)), m.group(3)))
    src = open('sub_main.cpp', 'rb').read().decode('latin-1')
    lines = src.split('\n')
    done = failed = 0
    for ln, cols in hits.items():
        s = lines[ln - 1]
        for col, kind in sorted(cols, reverse=True):
            try:
                s = rewrite_line(s, col, kind)
                done += 1
            except (ValueError, IndexError, AttributeError) as e:
                failed += 1
                print(f'{ln}:{col} {kind}: {e}')
        lines[ln - 1] = s
    print(f'{done} casts rewritten, {failed} left for hand editing, {len(hits)} lines')
    if not dry:
        open('sub_main.cpp', 'wb').write('\n'.join(lines).encode('latin-1'))


if __name__ == '__main__':
    main()

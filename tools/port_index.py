#!/usr/bin/env python3
"""Index of the port's game code: function definitions with their original (IDA) addresses and bodies.

Address rules (first hex number = this CD build):
  sub_41780_41AC0 / UpdateEntities_41780_41AC0 / sub_13170  -> 0x41780
  a preceding "//----- (00041780) ---" header wins; "//2126B0_" comments are IDA + 0x1D1000.
"""
import re
import sys

NAME_ADDR = re.compile(r'^(?:[A-Za-z]\w*?_)?(?:sub_)?([0-9A-F]{5})(?:_([0-9A-F]{5}))?$')
DEF_RE = re.compile(r'^(?!\s)(?!return\b|else\b|if\b|while\b|for\b|switch\b|case\b|typedef\b|#)[A-Za-z_][\w\s\*&:<>,]*?\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(?://\s*([0-9A-F]{6})_?.*)?\s*$')
HDR_RE = re.compile(r'^//----- \(([0-9A-F]{8})\)')


def strip_comments(text):
    """Blank out comments and string literals but keep line structure and offsets."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in text[i:j]))
            i = j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(c + ' ' * (j - i - 2) + c if j - i >= 2 else text[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def addr_from_name(name):
    m = NAME_ADDR.match(name)
    if not m:
        # names like DrawSprite_2FC50_2FC90 handled above; also Name_XXXXX at the end
        m2 = re.search(r'_([0-9A-F]{5})(?:_[0-9A-F]{5})?$', name)
        return int(m2.group(1), 16) if m2 else None
    return int(m.group(1), 16)


class PortIndex:
    def __init__(self, path='sub_main.cpp'):
        raw = open(path, 'rb').read().decode('latin-1')
        self.raw = raw
        self.code = strip_comments(raw)
        rlines = raw.split('\n')
        clines = self.code.split('\n')
        self.funcs = {}  # name -> dict(addr, start_line, end_line, body)
        i = 0
        last_hdr = None
        while i < len(clines):
            h = HDR_RE.match(rlines[i])
            if h:
                last_hdr = (i, int(h.group(1), 16))
            m = DEF_RE.match(clines[i].rstrip('\r'))
            if m and i + 1 < len(clines) and clines[i + 1].strip() == '{':
                name = m.group(1)
                rm = DEF_RE.match(rlines[i].rstrip('\r'))
                addr = addr_from_name(name)
                if last_hdr and i - last_hdr[0] <= 3:
                    addr = last_hdr[1]
                elif rm and rm.group(3):
                    addr = int(rm.group(3), 16) - 0x1D1000
                j = i + 2
                while j < len(clines) and not clines[j].startswith('}'):
                    j += 1
                body = '\n'.join(clines[i + 2:j])
                if name not in self.funcs:
                    self.funcs[name] = dict(addr=addr, line=i + 1, end=j + 1, body=body)
                i = j
            i += 1
        self.by_addr = {}
        for name, f in self.funcs.items():
            if f['addr'] is not None:
                self.by_addr.setdefault(f['addr'], []).append(name)

    def calls(self, name):
        body = self.funcs[name]['body']
        out = set()
        for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', body):
            callee = m.group(1)
            if callee in self.funcs and callee != name:
                out.add(callee)
        return out


if __name__ == '__main__':
    idx = PortIndex(sys.argv[1] if len(sys.argv) > 1 else 'sub_main.cpp')
    with_addr = [n for n, f in idx.funcs.items() if f['addr'] is not None]
    print(len(idx.funcs), 'functions,', len(with_addr), 'with addresses')
    for n in ('UpdateEntities_41780_41AC0', 'sub_13170', 'DrawSky_30730_30770', 'sub_416B0'):
        f = idx.funcs.get(n)
        print(n, hex(f['addr']) if f and f['addr'] is not None else f and f['addr'], f and f['line'])

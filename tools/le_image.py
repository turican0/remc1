#!/usr/bin/env python3
"""Loader for the DOS4GW-bound LE executable CARPET.EXE (Magic Carpet 1, CD build).

Builds the linked memory image the way IDA shows it: every object at its relocation base (code+const
at 0x10000, data at 0x90000, ...), with the LE fixup records applied. The raw file stores pointers as
offsets inside the target object; after `relocate()` they are absolute IDA addresses, and
`fixup_sites` tells which dwords were relocated (pointer vs. plain number - used by the audits).

    img = LeImage(remc1_paths.original_exe())
    mem = img.memory()            # dict: object base -> bytearray (relocated)
    img.read(0x9AD98, 4)          # relocated bytes at an IDA address
"""
import struct
import os
import remc1_paths  # noqa: E402
import sys


class LeImage:
    def __init__(self, path):
        exe = open(path, "rb").read()
        off = exe.find(b"LE\x00\x00")
        if off < 0:
            sys.exit("no LE header in " + path)
        self.exe = exe
        self.le = off
        h = lambda o, fmt="<I": struct.unpack_from(fmt, exe, off + o)[0]
        self.page_size = h(0x28)
        self.last_page_size = h(0x2C)
        self.npages = h(0x14)
        self.eip_obj, self.eip = h(0x18), h(0x1C)
        self.esp_obj, self.esp = h(0x20), h(0x24)
        objtab, nobj = h(0x40), h(0x44)
        self.pagemap = off + h(0x48)
        self.fpt = off + h(0x68)
        self.frt = off + h(0x6C)
        self.data_pages = h(0x80)
        self.objects = []  # (base, vsize, flags, first_page (1-based), page_count)
        for i in range(nobj):
            vsize, base, flags, pidx, pcnt, _ = struct.unpack_from("<6I", exe, off + objtab + i * 24)
            self.objects.append((base, vsize, flags, pidx, pcnt))
        self._mem = None
        self.fixup_sites = {}  # absolute source address -> (type, target absolute address)

    # ---- raw pages ------------------------------------------------------------------------------
    def page_bytes(self, page):  # page: 1-based logical page number
        start = self.data_pages + (page - 1) * self.page_size
        size = self.last_page_size if page == self.npages else self.page_size
        return self.exe[start:start + size]

    def memory(self):
        if self._mem is None:
            self._mem = {}
            for base, vsize, flags, pidx, pcnt in self.objects:
                buf = bytearray(vsize)
                for p in range(pcnt):
                    b = self.page_bytes(pidx + p)
                    buf[p * self.page_size:p * self.page_size + len(b)] = b
                self._mem[base] = buf
            self._apply_fixups()
        return self._mem

    def _obj_of_page(self, page):
        for i, (base, vsize, flags, pidx, pcnt) in enumerate(self.objects):
            if pidx <= page < pidx + pcnt:
                return i, (page - pidx) * self.page_size
        return None, None

    def _apply_fixups(self):
        exe = self.exe
        for page in range(1, self.npages + 1):
            a = self.frt + struct.unpack_from("<I", exe, self.fpt + (page - 1) * 4)[0]
            b = self.frt + struct.unpack_from("<I", exe, self.fpt + page * 4)[0]
            oi, page_off = self._obj_of_page(page)
            while a < b:
                src, flags = exe[a], exe[a + 1]
                a += 2
                stype = src & 0x0F
                if src & 0x20:
                    cnt = exe[a]
                    a += 1
                    srcoffs = None
                else:
                    srcoffs = [struct.unpack_from("<h", exe, a)[0]]
                    a += 2
                ttype = flags & 3
                if ttype != 0:
                    raise NotImplementedError("import fixups not expected in CARPET.EXE")
                if flags & 0x40:
                    tobj = struct.unpack_from("<H", exe, a)[0]
                    a += 2
                else:
                    tobj = exe[a]
                    a += 1
                toff = 0
                if stype != 2:  # selector fixups carry no offset
                    if flags & 0x10:
                        toff = struct.unpack_from("<I", exe, a)[0]
                        a += 4
                    else:
                        toff = struct.unpack_from("<H", exe, a)[0]
                        a += 2
                if srcoffs is None:
                    srcoffs = [struct.unpack_from("<h", exe, a + 2 * k)[0] for k in range(cnt)]
                    a += 2 * cnt
                if oi is None:
                    continue
                target = self.objects[tobj - 1][0] + toff
                base = self.objects[oi][0]
                buf = self._mem[base]
                for so in srcoffs:
                    pos = page_off + so
                    if stype == 7:  # 32-bit offset
                        if 0 <= pos and pos + 4 <= len(buf):
                            struct.pack_into("<I", buf, pos, target)
                        self.fixup_sites[base + pos] = (7, target)
                    elif stype == 8:  # 32-bit self-relative
                        if 0 <= pos and pos + 4 <= len(buf):
                            struct.pack_into("<i", buf, pos, target - (base + pos + 4))
                        self.fixup_sites[base + pos] = (8, target)
                    elif stype == 2:  # 16-bit selector: flat model, nothing to patch
                        self.fixup_sites[base + pos] = (2, target)
                    elif stype == 5:  # 16-bit offset
                        if 0 <= pos and pos + 2 <= len(buf):
                            struct.pack_into("<H", buf, pos, target & 0xFFFF)
                        self.fixup_sites[base + pos] = (5, target)
                    else:
                        self.fixup_sites[base + pos] = (stype, target)

    def object_at(self, addr):
        for base, vsize, flags, pidx, pcnt in self.objects:
            if base <= addr < base + vsize:
                return base
        return None

    def read(self, addr, length):
        mem = self.memory()
        base = self.object_at(addr)
        if base is None:
            raise ValueError(f"{addr:#x} outside every object")
        o = addr - base
        return bytes(mem[base][o:o + length])

    def code_range(self):
        base, vsize = self.objects[0][0], self.objects[0][1]
        return base, base + vsize


if __name__ == "__main__":
    img = LeImage(sys.argv[1] if len(sys.argv) > 1 else remc1_paths.original_exe())
    for i, o in enumerate(img.objects):
        print(f"object {i + 1}: base {o[0]:#x} size {o[1]:#x} flags {o[2]:#x} pages {o[3]}+{o[4]}")
    img.memory()
    print("fixups:", len(img.fixup_sites), "entry", hex(img.objects[img.eip_obj - 1][0] + img.eip))
    # sanity: the code read [0x1e408] in the raw file = dword_AE408 after relocation
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for ins in md.disasm(img.read(0x5A0A5, 12), 0x5A0A5):
        print(f"{ins.address:06X}: {ins.mnemonic} {ins.op_str}")

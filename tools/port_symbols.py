#!/usr/bin/env python3
"""Global symbols of the built port (remc1.exe + remc1.pdb) and their initial bytes.

Uses dbghelp (Windows) to enumerate the PDB's data symbols (name, RVA, size) and reads the initial
contents from the PE image (.data/.rdata; uninitialised .bss reads as zeros). PE base relocations tell
which dwords are pointers.

    ps = PortSymbols(remc1_paths.find_port_exe())
    for s in ps.data_symbols(): s.name, s.rva, s.size
    ps.initial_bytes(rva, size)
"""
import ctypes
import ctypes.wintypes as wt
import os
import remc1_paths  # noqa: E402
import struct
import sys

SYMOPT_UNDNAME = 0x2
SYMOPT_DEFERRED_LOADS = 0x4
SYMOPT_LOAD_LINES = 0x10
SymTagData = 7
SymTagFunction = 5
TI_GET_LENGTH = 2
TI_GET_TYPE = 4


class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wt.ULONG), ('TypeIndex', wt.ULONG), ('Reserved', ctypes.c_uint64 * 2),
                ('Index', wt.ULONG), ('Size', wt.ULONG), ('ModBase', ctypes.c_uint64), ('Flags', wt.ULONG),
                ('Value', ctypes.c_uint64), ('Address', ctypes.c_uint64), ('Register', wt.ULONG),
                ('Scope', wt.ULONG), ('Tag', wt.ULONG), ('NameLen', wt.ULONG), ('MaxNameLen', wt.ULONG),
                ('Name', ctypes.c_char * 1024)]


class Sym:
    __slots__ = ('name', 'rva', 'size', 'tag', 'type_index')

    def __init__(self, name, rva, size, tag, ti):
        self.name, self.rva, self.size, self.tag, self.type_index = name, rva, size, tag, ti

    def __repr__(self):
        return f'Sym({self.name}, rva={self.rva:#x}, size={self.size})'


class PortSymbols:
    BASE = 0x10000000
    STATICS = 0x400000  # where the statics are in the game's 32-bit offset space: 4 MB + RVA (engine/ptr32.cpp)

    def __init__(self, exe):
        self.exe = os.path.abspath(exe)
        self.pe = open(self.exe, 'rb').read()
        self._parse_pe()
        self._load_syms()

    # ---- PE ----------------------------------------------------------------------------------------
    def _parse_pe(self):
        pe = self.pe
        e_lfanew = struct.unpack_from('<I', pe, 0x3C)[0]
        nsec = struct.unpack_from('<H', pe, e_lfanew + 6)[0]
        optsize = struct.unpack_from('<H', pe, e_lfanew + 20)[0]
        opt = e_lfanew + 24
        if struct.unpack_from('<H', pe, opt)[0] != 0x20B:
            raise OSError(f'{exe}: not a 64-bit (PE32+) executable')
        self.pointer_size = 8
        self.image_base = struct.unpack_from('<Q', pe, opt + 24)[0]
        dd = opt + 112
        self.data_dirs = dd
        reloc_rva, reloc_size = struct.unpack_from('<II', pe, dd + 5 * 8)
        self.sections = []
        so = opt + optsize
        for i in range(nsec):
            name = pe[so:so + 8].rstrip(b'\0').decode()
            vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', pe, so + 8)
            self.sections.append((name, va, vsize, rawptr, rawsize))
            so += 40
        self.reloc64_sites = set()  # 8-byte absolute addresses
        if reloc_rva:
            p = self.rva_to_off(reloc_rva)
            end = p + reloc_size
            while p < end:
                page, blk = struct.unpack_from('<II', pe, p)
                if blk == 0:
                    break
                for k in range((blk - 8) // 2):
                    e = struct.unpack_from('<H', pe, p + 8 + 2 * k)[0]
                    if e >> 12 == 10:  # DIR64
                        self.reloc64_sites.add(page + (e & 0xFFF))
                p += blk

    def rva_to_off(self, rva):
        for name, va, vsize, rawptr, rawsize in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                o = rva - va
                return rawptr + o if o < rawsize else None
        return None

    def section_of(self, rva):
        for name, va, vsize, rawptr, rawsize in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                return name
        return None

    def initial_bytes(self, rva, size):
        out = bytearray(size)
        for i in range(size):
            o = self.rva_to_off(rva + i)
            if o is not None and o < len(self.pe):
                out[i] = self.pe[o]
        return bytes(out)

    # ---- PDB ---------------------------------------------------------------------------------------
    def _load_syms(self):
        if os.name != 'nt':  # the PDB of the MSVC build is read through Windows' dbghelp
            raise OSError('port symbols need Windows (dbghelp + the MSVC .pdb); the diff harness is Windows-only for now')
        dbg = ctypes.WinDLL('dbghelp.dll')
        self.dbg = dbg
        self.h = wt.HANDLE(0x7357)
        dbg.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS)
        dbg.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
        if not dbg.SymInitialize(self.h, os.path.dirname(self.exe).encode(), False):
            raise OSError('SymInitialize failed')
        dbg.SymLoadModuleEx.restype = ctypes.c_uint64
        dbg.SymLoadModuleEx.argtypes = [wt.HANDLE, wt.HANDLE, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint64,
                                        wt.DWORD, ctypes.c_void_p, wt.DWORD]
        base = dbg.SymLoadModuleEx(self.h, None, self.exe.encode(), None, self.BASE, len(self.pe), None, 0)
        if not base:
            raise OSError('SymLoadModuleEx failed')
        self.syms = []
        CB = ctypes.WINFUNCTYPE(wt.BOOL, ctypes.POINTER(SYMBOL_INFO), wt.ULONG, ctypes.c_void_p)

        def cb(pinfo, size, ctx):
            si = pinfo.contents
            name = si.Name[:si.NameLen].decode('latin-1')
            self.syms.append(Sym(name, si.Address - self.BASE, si.Size, si.Tag, si.TypeIndex))
            return True

        self._cb = CB(cb)
        dbg.SymEnumSymbols.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.c_char_p, CB, ctypes.c_void_p]
        dbg.SymEnumSymbols(self.h, self.BASE, b'*', self._cb, None)
        dbg.SymGetTypeInfo.argtypes = [wt.HANDLE, ctypes.c_uint64, wt.ULONG, ctypes.c_int, ctypes.c_void_p]
        for s in self.syms:
            if s.tag == SymTagData and s.size == 0 and s.type_index:
                ln = ctypes.c_uint64(0)
                if dbg.SymGetTypeInfo(self.h, self.BASE, s.type_index, TI_GET_LENGTH, ctypes.byref(ln)):
                    s.size = ln.value

    # ---- types: where are the pointers? ---------------------------------------------------------
    def _ti(self, ti, what, ctype=ctypes.c_uint32):
        v = ctype(0)
        ok = self.dbg.SymGetTypeInfo(self.h, self.BASE, ti, what, ctypes.byref(v))
        return v.value if ok else None

    def pointer_fields(self, ti):
        """Pointer fields inside an object of type ti: [(offset, width, real)]. real=True: a machine pointer
        (8 bytes); real=False: a ptr32<T> field, which holds a 4-byte game offset (engine/ptr32.h)."""
        if not hasattr(self, '_pf_cache'):
            self._pf_cache = {}
        if ti in self._pf_cache:
            return self._pf_cache[ti]
        self._pf_cache[ti] = ()  # recursion guard
        TI_GET_SYMTAG, TI_GET_SYMNAME, TI_GET_TYPEID, TI_FINDCHILDREN, TI_GET_OFFSET, TI_GET_COUNT, TI_GET_CHILDRENCOUNT =             0, 1, 4, 7, 10, 12, 13
        tag = self._ti(ti, TI_GET_SYMTAG)
        out = ()
        if tag == 14:  # pointer
            out = ((0, self.pointer_size, True),)
        elif tag in (17, 7):  # typedef / data member -> underlying type
            sub = self._ti(ti, TI_GET_TYPEID)
            out = self.pointer_fields(sub) if sub else ()
        elif tag == 15:  # array
            elem = self._ti(ti, TI_GET_TYPEID)
            count = self._ti(ti, TI_GET_COUNT) or 0
            eo = self.pointer_fields(elem) if elem else ()
            if eo:
                ln = ctypes.c_uint64(0)
                self.dbg.SymGetTypeInfo(self.h, self.BASE, elem, TI_GET_LENGTH, ctypes.byref(ln))
                out = tuple((k * ln.value + o, w, r) for k in range(count) for o, w, r in eo)
        elif tag == 11:  # struct / union / class
            p = ctypes.c_wchar_p()
            name = p.value if self.dbg.SymGetTypeInfo(self.h, self.BASE, ti, TI_GET_SYMNAME, ctypes.byref(p)) else ''
            if name and name.startswith('ptr32<'):
                out = ((0, 4, False),)
            elif name and name.startswith('extptr32<'):
                out = ()  # a handle, not an address
            else:
                n = self._ti(ti, TI_GET_CHILDRENCOUNT) or 0
                if n:
                    class FC(ctypes.Structure):
                        _fields_ = [('Count', ctypes.c_uint32), ('Start', ctypes.c_uint32), ('ChildId', ctypes.c_uint32 * n)]
                    fc = FC(n, 0)
                    if self.dbg.SymGetTypeInfo(self.h, self.BASE, ti, TI_FINDCHILDREN, ctypes.byref(fc)):
                        fields = set()
                        for c in fc.ChildId:
                            if self._ti(c, TI_GET_SYMTAG) != 7:
                                continue
                            mo, mt = self._ti(c, TI_GET_OFFSET), self._ti(c, TI_GET_TYPEID)
                            if mo is None or mt is None:
                                continue
                            fields.update((mo + o, w, r) for o, w, r in self.pointer_fields(mt))
                        out = tuple(sorted(fields))
        self._pf_cache[ti] = out
        return out

    def project32(self, sym, data, conv):
        """A global's bytes in the 32-bit layout of the original: every real 8-byte pointer becomes a 4-byte
        offset (conv(value); 0 if conv returns None), everything else is kept (the game's structs are packed).
        Returns (bytes, segments [(projected offset, port offset, length)] or None if the layout is unchanged,
        [projected offsets of the 4-byte pointer fields: converted real pointers and ptr32<T> fields])."""
        fields = self.pointer_fields(sym.type_index) if sym.type_index else ()
        real = [o for o, w, r in fields if r and w == 8]
        if not real:
            return data, None, [o for o, w, r in fields if not r]
        out, segs, pos = bytearray(), [], 0
        for o in real:
            if o < pos or o + 8 > len(data):
                continue
            segs.append((len(out), pos, o - pos))
            out += data[pos:o]
            v = struct.unpack_from('<Q', data, o)[0]
            t = conv(v) if v else 0
            segs.append((len(out), o, 4))
            out += struct.pack('<I', (t or 0) & 0xFFFFFFFF)
            pos = o + 8
        segs.append((len(out), pos, len(data) - pos))
        out += data[pos:]
        offs = []
        for o, w, r in fields:
            for po, so, n in segs:
                if so <= o < so + max(n, 1) and (n == 4 or not r):
                    offs.append(po + o - so)
                    break
        return bytes(out), segs, offs

    def signature(self, sym):
        """(return kind, [parameter kinds]) of a function symbol, each 'ptr' (a machine pointer), 'void' or
        'int' (anything else), from its PDB function type; None if the PDB has no type for it."""
        TI_GET_SYMTAG, TI_GET_TYPEID, TI_FINDCHILDREN, TI_GET_CHILDRENCOUNT = 0, 4, 7, 13

        def kind(ti):
            while ti and self._ti(ti, TI_GET_SYMTAG) == 17:  # typedef
                ti = self._ti(ti, TI_GET_TYPEID)
            tag = self._ti(ti, TI_GET_SYMTAG) if ti else None
            if tag == 14:
                return 'ptr'
            if tag == 16 and self._ti(ti, 5) == 1:  # TI_GET_BASETYPE btVoid
                return 'void'
            return 'int'
        ft = sym.type_index
        if not ft or self._ti(ft, TI_GET_SYMTAG) != 13:  # FunctionType
            return None
        ret = kind(self._ti(ft, TI_GET_TYPEID))
        n = self._ti(ft, TI_GET_CHILDRENCOUNT) or 0
        args = []
        if n:
            class FC(ctypes.Structure):
                _fields_ = [('Count', ctypes.c_uint32), ('Start', ctypes.c_uint32), ('ChildId', ctypes.c_uint32 * n)]
            fc = FC(n, 0)
            if self.dbg.SymGetTypeInfo(self.h, self.BASE, ft, TI_FINDCHILDREN, ctypes.byref(fc)):
                args = [kind(self._ti(c, TI_GET_TYPEID)) for c in fc.ChildId if self._ti(c, TI_GET_SYMTAG) == 20]
        return ret, args

    def members(self, ti):
        """Data members of a struct type: [(name, offset, type index, size)] (empty for other types)."""
        TI_GET_SYMTAG, TI_GET_SYMNAME, TI_GET_TYPEID, TI_FINDCHILDREN, TI_GET_OFFSET, TI_GET_CHILDRENCOUNT = 0, 1, 4, 7, 10, 13
        while self._ti(ti, TI_GET_SYMTAG) == 17:  # typedef
            ti = self._ti(ti, TI_GET_TYPEID)
        if self._ti(ti, TI_GET_SYMTAG) != 11:
            return []
        n = self._ti(ti, TI_GET_CHILDRENCOUNT) or 0
        if not n:
            return []

        class FC(ctypes.Structure):
            _fields_ = [('Count', ctypes.c_uint32), ('Start', ctypes.c_uint32), ('ChildId', ctypes.c_uint32 * n)]
        fc = FC(n, 0)
        if not self.dbg.SymGetTypeInfo(self.h, self.BASE, ti, TI_FINDCHILDREN, ctypes.byref(fc)):
            return []
        out = []
        for c in fc.ChildId:
            if self._ti(c, TI_GET_SYMTAG) != 7:
                continue
            p = ctypes.c_wchar_p()
            if not self.dbg.SymGetTypeInfo(self.h, self.BASE, c, TI_GET_SYMNAME, ctypes.byref(p)) or not p.value:
                continue
            name = p.value
            off, mt = self._ti(c, TI_GET_OFFSET), self._ti(c, TI_GET_TYPEID)
            ln = ctypes.c_uint64(0)
            self.dbg.SymGetTypeInfo(self.h, self.BASE, mt, TI_GET_LENGTH, ctypes.byref(ln))
            out.append((name, off, mt, ln.value))
        return out

    def data_symbols(self):
        return [s for s in self.syms if s.tag == SymTagData]

    def by_name(self):
        return {s.name: s for s in self.syms}


if __name__ == '__main__':
    ps = PortSymbols(sys.argv[1] if len(sys.argv) > 1 else remc1_paths.find_port_exe())
    d = ps.data_symbols()
    print(len(ps.syms), 'symbols,', len(d), 'data;', len(ps.reloc64_sites), 'relocations')
    bn = ps.by_name()
    for n in ('creatureTypes_98F38', 'str_9688C', 'perfFont_96894', 'dword_999B8', 'mapHeightmap_DC1E0_DC1D0'):
        s = bn.get(n)
        print(n, s, s and ps.section_of(s.rva))

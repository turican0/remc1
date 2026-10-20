#!/usr/bin/env python3
"""Unicorn machine holding a port memory snapshot, the original CARPET.EXE and the port's own code.

Both the ORIGINAL function (IDA address) and the PORT function (remc1.exe, relocated to the snapshot's
image base) can be called on the same memory; every write is logged so a call can be undone. Port
imports (CRT) are served by Python stubs. Used by tools/diff_tick.py and tools/diff_funcs.py.
"""
import bisect
import os
import remc1_paths  # noqa: E402
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_PROT_ALL, UC_PROT_READ, UC_HOOK_INTR, UC_HOOK_MEM_INVALID, \
    UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, \
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP
from le_image import LeImage
from port_symbols import PortSymbols, SymTagFunction
from audit_data import PortAddrMap, name_addresses

PAGE = 0x1000
IDA_LO, IDA_HI = 0x10000, 0x150000
STACK_BASE, STACK_SIZE = 0x7F000000, 0x100000
TRAP = 0x7E000000
STUBS = 0x7D000000
HEAP_BASE, HEAP_SIZE = 0x7A000000, 0x2000000
TEB64 = 0x7C000000  # 64-bit port machine: a minimal thread environment block (GS base)

SKIP_GLOBALS = {'tempScreenBuffer_A0000', 'unk_93AE0', 'off_96864', 'videoModes_12F080', 'pathStrArray_F1EF0',
                'unk_A1D34', 'str_254DBC', 'dword_999B8', 'off_9EF10', 'off_99A84', 'off_99B68',
                # pointer into sprite data whose header layout differs between the two programs; always set before use
                'sprData_B5CB0_B5CA0'}
CLIP_GLOBALS = {'dword_9334C': 1024}
# original bytes without a port counterpart that are no behaviour: the high words of the viewport width and
# height (dwords in the original, uint16 in the port)
# and the font table pointers at 0xAE3B8 (two dwords in the original, the struct array dword_AE3B8_2 in the port)
LAYOUT_IGNORE_IDA = [(0x93ADA, 0x93ADC), (0x93ADE, 0x93AE0), (0xAE3B8, 0xAE3C0)]
LAYOUT_IGNORE_PORT = ['dword_AE3B8_2']  # whole port symbols on the port side (12-byte font structs)
# sound: the original drives the HMI/SOS driver, the port SDL_mixer - neither is game logic
SKIP_FUNCS = ['sub_55370_558A0', 'sub_55890_55DC0',
              # port-only diagnostics called from game functions (the snapshot hooks themselves)
              'MemSnapshot_Call', 'MemSnapshot_Tick']


def read_dump(path):
    regions = []
    with open(path, 'rb') as f:
        assert f.read(8) == b'MC1SNAP1'
        while True:
            base, size = struct.unpack('<II', f.read(8))
            if size == 0:
                break
            regions.append((base, f.read(size)))
    return regions


def read_meta(path):
    meta = {}
    for line in open(path):
        k, *v = line.split()
        meta[k] = [int(x, 0) for x in v]
    return meta


class Dump:
    def __init__(self, regions):
        self.regions = sorted(regions)
        self.bases = [r[0] for r in self.regions]

    def read(self, addr, n):
        """Bytes [addr, addr+n) - may span adjacent regions; None if any byte is missing."""
        out = bytearray()
        while n > 0:
            i = bisect.bisect_right(self.bases, addr) - 1
            if i < 0:
                return None
            base, data = self.regions[i]
            o = addr - base
            if o >= len(data):
                return None
            take = min(n, len(data) - o)
            out += data[o:o + take]
            addr += take
            n -= take
        return bytes(out)


class Stop(Exception):
    pass


class Machine:
    def __init__(self, snapdir, exe=None, port=None,
                 load_port_code=True):
        exe = exe or remc1_paths.original_exe()
        port = port or os.environ.get('REMC1_PORT_EXE') or remc1_paths.find_port_exe()
        self.meta = read_meta(os.path.join(snapdir, 'meta.txt'))
        self.image_base = self.meta['image_base'][0]
        # snapshots are in the game's 32-bit offset space (portability/MemSnapshot.cpp); real pointers in the
        # port's statics become offsets through these window bases (statics, arena, stack)
        if self.meta.get('pointer_size', [4])[0] != 8 or 'bases' not in self.meta:
            raise SystemExit(f'{snapdir}: not a snapshot of a 64-bit build (take a new one)')
        self.bases = self.meta['bases']
        self.state_addr, self.state_size = self.meta['state']
        self.before = read_dump(os.path.join(snapdir, 'before.bin'))
        self.bdump = Dump(self.before)
        a_path = os.path.join(snapdir, 'after.bin')
        self.after = Dump(read_dump(a_path)) if os.path.exists(a_path) else None
        self.img = LeImage(exe)
        self.omem = self.img.memory()
        self.ps = PortSymbols(port)
        self.amap = PortAddrMap(self.ps, self.img)
        self.sym_by_name = self.ps.by_name()
        # port-only instrumentation globals (call counters etc.): writes to them are not differences
        self.port_only = set()
        for s_ in self.ps.data_symbols():
            if s_.name.startswith(('counter_', 'compare_', 'debugcounter')) or s_.name == 'DrawGameFrame_20FB0_index':
                self.port_only.update(range(self.image_base + s_.rva, self.image_base + s_.rva + max(s_.size, 1)))
        ib = self.image_base
        self.image_lo = ib
        self.image_hi = ib + max(s[1] + max(s[2], s[4]) for s in self.ps.sections)
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.mapped = []
        for base, data in sorted(self.before):
            lo, hi = base, base + len(data)
            if (hi <= IDA_LO or lo >= IDA_HI) and lo < HEAP_BASE:
                mu.mem_map(lo, hi - lo, UC_PROT_ALL)
                mu.mem_write(lo, data)
                self.mapped.append((lo, hi))
        mu.mem_map(IDA_LO, IDA_HI - IDA_LO, UC_PROT_ALL)
        for (obase, vsize, flags, pidx, pcnt) in self.img.objects:
            mu.mem_write(obase, bytes(self.omem[obase]))
        if not any(lo < PAGE for lo, _ in self.mapped):
            # DOS low memory (IVT/BIOS data) is readable under DOS/4GW, and the original reads it
            # through null entity pointers (docs/original-bugs.md #3): map it read-only, zeroed
            mu.mem_map(0, PAGE, UC_PROT_READ)
        mu.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_ALL)
        mu.mem_map(TRAP, PAGE, UC_PROT_ALL)
        mu.mem_write(TRAP, b'\xf4' * 16)
        mu.mem_map(STUBS, 0x10000, UC_PROT_ALL)
        mu.mem_write(STUBS, b'\xc3' * 0x10000)
        mu.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
        self.heap_ptr = HEAP_BASE
        self.stub_funcs = {}
        self.mu64 = None  # the port's code runs in a second, 64-bit machine (_init_port64)
        self.dirty = set()
        if load_port_code:
            self._init_port64()
        self._copy_globals()
        self.wlog = []
        self.logging = False
        self.watch = set()  # addresses whose writes are printed (diff_tick.py --watch)
        mu.hook_add(UC_HOOK_INTR, self._on_intr)
        mu.hook_add(UC_HOOK_MEM_INVALID, self._on_invalid)
        mu.hook_add(UC_HOOK_CODE, self._on_trap, begin=TRAP, end=TRAP + 16)
        mu.hook_add(UC_HOOK_CODE, self._on_stub, begin=STUBS, end=STUBS + 0xFFFF)
        mu.hook_add(UC_HOOK_MEM_WRITE, self._on_write)
        self.stop_reason = None
        # functions skipped on BOTH sides (platform code the port replaced): original address + port name
        self.skipped = {}
        for name in SKIP_FUNCS:
            from port_index import addr_from_name
            oa = addr_from_name(name)
            if oa is not None:
                self.skip_function(oa, 0)
            if name in self.sym_by_name:
                self.skip_function(self.port_addr(name), 0)
        self.ctx = mu.context_save()  # clean CPU state (segments!) restored before every call

    def skip_function(self, addr, retval=0):
        """Make every call to addr return immediately with eax = retval."""
        if self.mu64 is not None and self.image_lo <= addr < self.image_hi:
            from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RAX, UC_X86_REG_RIP

            def hook64(uc, address, size, user):
                rsp = uc.reg_read(UC_X86_REG_RSP)
                ret = struct.unpack('<Q', bytes(uc.mem_read(rsp, 8)))[0]
                uc.reg_write(UC_X86_REG_RSP, rsp + 8)
                uc.reg_write(UC_X86_REG_RAX, retval)
                uc.reg_write(UC_X86_REG_RIP, ret)
            ra = self.real_addr(addr)
            self.skipped[addr] = self.mu64.hook_add(UC_HOOK_CODE, hook64, begin=ra, end=ra)
            return
        def hook(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', bytes(uc.mem_read(esp, 4)))[0]
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, retval)
            uc.reg_write(UC_X86_REG_EIP, ret)
        self.skipped[addr] = self.mu.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)

    # ---- setup -----------------------------------------------------------------------------------
    def _global_syms(self):
        """Data symbols, plus the address-named members of struct globals that have no address in their
        own name (e.g. strPal.fog_B7934_B7924: the renderer's fog/blend tables)."""
        from port_symbols import Sym, SymTagData
        out = list(self.ps.data_symbols())
        self.member_syms = set()
        for s in self.ps.data_symbols():
            if s.size < 64 or name_addresses(s.name, self.img) or s.name in SKIP_GLOBALS:
                continue
            for mname, off, mt, msize in self.ps.members(s.type_index):
                if msize and name_addresses(mname, self.img):
                    out.append(Sym(mname, s.rva + off, msize, SymTagData, mt))
                    self.member_syms.add(mname)
        return out

    def _copy_globals(self):
        """Port globals named after original addresses -> original addresses (pointers translated)."""
        mu = self.mu
        self.global_map = []  # (ida, size, port_addr, name)
        self.gsegs = {}  # name -> projection segments (64-bit builds, see project())
        # bigger globals first: port arrays that were enlarged (or IDA "tables" that are really several
        # variables) cover other globals; the more specific (smaller) global must win
        owner = {}  # ida byte -> (static match, global) of the global currently owning it
        self.dropped = []
        # linker-like layout: a global ends where the next address-named global begins (port arrays
        # were often enlarged - word_CC070 ran into mapTerrainType_CC1E0 - and IDA "tables" such as
        # trig_9074C cover separately named variables). Static data cut off this way keeps the
        # original's own (identical) bytes.
        gsyms = self._global_syms()
        spans = []
        for s in gsyms:
            c = name_addresses(s.name, self.img)
            if c and s.size:
                spans.append((c[0], s.size, s.name))
        # an unk_ placeholder lying entirely inside another global (e.g. unk_110003 inside the 128 KB
        # mapEntityIndex) is IDA junk, not a variable: neither a boundary nor copied
        self.junk = set()
        # The same holds for a name that only carries a DOSBox-style 6-digit address (snds00Tab_2854A1 ->
        # 0xC74A1 lies inside the 64 KB blend table strPal.byte_BB934): it names other memory.
        import re
        for a0, n0, nm in spans:
            toks = [int(t, 16) for t in re.findall(r'_([0-9A-F]{5,6})(?=_|$)', nm)]
            dosbox_only = bool(toks) and all(t >= 0x200000 for t in toks)  # DOSBox = IDA + 0x1BE000
            # (only inside struct-member tables: enlarged port arrays would otherwise swallow e.g. begWscreen)
            inside = [n2 for a1, n1, n2 in spans if a1 < a0 and a0 + n0 <= a1 + n1]
            if (nm.startswith('unk_') and inside) or (dosbox_only and any(n2 in self.member_syms for n2 in inside)):
                self.junk.add(nm)
        # skipped globals are no boundary either (pathStrArray_F1EF0 would cut the 64 KB mapShading_EC1E0)
        starts = sorted({a0 for a0, n0, nm in spans if nm not in self.junk and nm not in SKIP_GLOBALS})
        self.clip_end = {}
        for i, a0 in enumerate(starts[:-1]):
            self.clip_end[a0] = starts[i + 1]
        for s in sorted(gsyms, key=lambda s: -s.size):
            if s.name in SKIP_GLOBALS or s.size == 0 or s.name in self.junk:
                continue
            cands = name_addresses(s.name, self.img)
            if not cands:
                continue
            ida = cands[0]
            obase = self.img.object_at(ida)
            oend = obase + [o for o in self.img.objects if o[0] == obase][0][1]
            data = self.bdump.read(self.image_base + s.rva, s.size)
            if data is None:
                continue
            data, segs = self.project(s, data, self.real_to_off)
            size = min(len(data), oend - ida, CLIP_GLOBALS.get(s.name, s.size), self.clip_end.get(ida, ida + s.size) - ida)
            data = data[:size]
            # does the port's static image agree with the original data at that address? A global whose
            # name carries a wrong address (other build) disagrees with the bytes it would cover.
            pinit = self.project(s, self.ps.initial_bytes(s.rva, s.size),
                                 lambda v: v - self.ps.image_base + self.image_base)[0][:size]
            oinit = bytes(self.omem[obase][ida - obase:ida - obase + size])
            nz = [i for i in range(size) if oinit[i] or pinit[i]]
            match = 1.0 if not nz else sum(1 for i in nz if oinit[i] == pinit[i]) / len(nz)
            overlapped = [owner[ida + i] for i in range(size) if ida + i in owner]
            if overlapped and any(m0 > match + 0.2 for m0, _ in overlapped):
                self.dropped.append((s.name, ida, size, match, overlapped[0][1]))
                continue
            for i in range(size):
                owner[ida + i] = (match, s.name)
            mu.mem_write(ida, self.translate_block(data, s.rva, s, segs))
            self.global_map.append((ida, size, self.image_base + s.rva, s.name))
            if segs:
                self.gsegs[s.name] = segs
        self.global_map.sort()
        # skipped globals (scratch buffers, platform state): writes to them are no differences
        self.skip_ida, self.skip_port = list(LAYOUT_IGNORE_IDA), []
        for s in self.ps.data_symbols():
            if s.name in SKIP_GLOBALS and s.size:
                self.skip_port.append((self.image_base + s.rva, self.image_base + s.rva + s.size))
                c = name_addresses(s.name, self.img)
                if c:  # clipped like the copies (tempScreenBuffer_A0000 is far bigger in the port)
                    nxt = min([a0 for a0 in self.clip_end.values() if a0 > c[0]] or [c[0] + s.size])
                    self.skip_ida.append((c[0], min(c[0] + s.size, nxt)))
        self.gm_pages = {}
        for g in self.global_map:
            for pg in range(g[0] >> 12, ((g[0] + g[1] - 1) >> 12) + 1):
                self.gm_pages.setdefault(pg, []).append(g)
        # the port side of the known layout differences
        for n in LAYOUT_IGNORE_PORT:
            sym = self.sym_by_name.get(n)
            if sym:
                self.skip_port.append((self.image_base + sym.rva, self.image_base + sym.rva + sym.size))
        for lo, hi in LAYOUT_IGNORE_IDA:
            for a in range(lo, hi):
                pa, _ = self.ida_to_port(a)
                if pa is not None:
                    self.skip_port.append((pa, pa + 1))

    # ---- 64-bit builds: statics holding real (8-byte) pointers, seen in the 32-bit layout ------------------
    def real_to_off(self, v):
        """real pointer of the snapshotted process -> game offset (None outside the windows)."""
        b0, b1, b2 = self.bases
        if 0 <= v - b0 < 0x20000000:
            return v - b0
        if 0x20000000 <= v - b1 < 0x40000000:
            return v - b1
        if 0x40000000 <= v - b2 < 0x80000000:
            return v - b2
        return None

    def project(self, sym, data, conv):
        """The bytes of a port global in the 32-bit layout the original expects (PortSymbols.project32):
        (bytes, segments or None)."""
        d, segs, _ = self.ps.project32(sym, data, conv)
        return d, segs

    @staticmethod
    def seg_port_offset(segs, rel):
        """projected offset -> offset in the port global (see project())."""
        for po, so, n in segs:
            if po <= rel < po + n:
                return so + rel - po
        return None

    def translate_value(self, v):
        """port pointer -> original address (or None)."""
        if self.image_lo <= v < self.image_hi:
            sym, tida = self.amap.lookup(v - self.image_base)
            if tida is not None and (sym.tag == SymTagFunction or sym.name not in SKIP_GLOBALS):
                return tida
        return None

    def translate_block(self, data, rva=None, sym=None, segs=None):
        """Translate port pointers inside a global's bytes to original addresses - only at the pointer
        fields the PDB type of the global declares (guessing by value turned packed table words and
        entity-index pairs into fake pointers). Falls back to relocation sites without a type.
        data is in the 32-bit layout (project(), segs): every pointer field is 4 bytes."""
        buf = bytearray(data)
        offs = []
        if sym is not None and sym.type_index:
            offs = []
            for o, w, real in self.ps.pointer_fields(sym.type_index):
                if not segs:
                    offs.append(o)
                else:
                    for po, so, n in segs:
                        if so <= o < so + max(n, 1) and (n == 4 or not real):
                            offs.append(po + o - so)
                            break
        for o in offs:
            if o + 4 > len(buf):
                continue
            v = struct.unpack_from('<I', buf, o)[0]
            if self.image_lo <= v < self.image_hi:
                t = self.translate_value(v)
                if t is not None:
                    struct.pack_into('<I', buf, o, t)
        return bytes(buf)

    def ida_to_port(self, a):
        best = None
        for g in self.gm_pages.get(a >> 12, ()):
            if g[0] <= a < g[0] + g[1] and (best is None or g[1] < best[1]):
                best = g
        if best:
            ida, size, paddr, name = best
            segs = self.gsegs.get(name)
            if segs:
                o = self.seg_port_offset(segs, a - ida)
                return (None, None) if o is None else (paddr + o, name)
            return paddr + (a - ida), name
        return None, None

    # ---- hooks -----------------------------------------------------------------------------------
    def _on_intr(self, uc, intno, user):
        self.stop_reason = f'interrupt {intno:#x} at {uc.reg_read(UC_X86_REG_EIP):#x}'
        uc.emu_stop()

    def _on_invalid(self, uc, access, address, size, value, user):
        self.stop_reason = f'invalid memory access (type {access}) at {address:#x}, eip {uc.reg_read(UC_X86_REG_EIP):#x}'
        return False

    def _on_trap(self, uc, address, size, user):
        uc.emu_stop()

    def _on_write(self, uc, access, address, size, value, user):
        if self.logging:
            self.wlog.append((address, bytes(uc.mem_read(address, size))))
        if self.mu64 is not None:
            self.dirty.add(address >> 12)
            self.dirty.add((address + size - 1) >> 12)
        if self.watch and any(address <= w < address + size for w in self.watch):
            eip = uc.reg_read(UC_X86_REG_EIP)
            esp = uc.reg_read(UC_X86_REG_ESP)
            stack = struct.unpack('<64I', bytes(uc.mem_read(esp, 256)))
            rets = [hex(v) for v in stack if 0x10000 <= v < 0x90000][:8]
            if hasattr(self, 'watch_log'):
                self.watch_log.append((address, eip, value))
            print(f'  write {address:#x} size {size} value {value & ((1 << 8 * size) - 1):#x} at eip {eip:#x}; '
                  f'stack code addresses {" ".join(rets)}')

    def arg(self, n):
        esp = self.mu.reg_read(UC_X86_REG_ESP)
        return struct.unpack('<I', bytes(self.mu.mem_read(esp + 4 + 4 * n, 4)))[0]

    def _on_stub(self, uc, address, size, user):
        name = self.stub_funcs.get(address, '?')
        a = self.arg
        r = 0
        if name in ('memcpy', 'memmove'):
            d, s, n = a(0), a(1), a(2)
            if n:
                data = bytes(uc.mem_read(s, n))
                self._logged_write(d, data)
            r = d
        elif name == 'memset':
            d, c, n = a(0), a(1), a(2)
            if n:
                self._logged_write(d, bytes([c & 0xFF]) * n)
            r = d
        elif name in ('malloc', 'calloc'):
            n = a(0) * (a(1) if name == 'calloc' else 1)
            r = self.heap_ptr
            self.heap_ptr += (n + 15) & ~15
        elif name in ('free', '__stdio_common_vfprintf', '__stdio_common_vsprintf', 'fflush', 'fputs', 'fputc',
                      '__acrt_iob_func', 'fwrite'):
            r = 0
        elif name in ('_libm_sse2_pow_precise',):
            from unicorn.x86_const import UC_X86_REG_XMM0, UC_X86_REG_XMM1
            x0 = uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)
            x1 = uc.reg_read(UC_X86_REG_XMM1) & ((1 << 64) - 1)
            a_, b_ = struct.unpack('<d', struct.pack('<Q', x0))[0], struct.unpack('<d', struct.pack('<Q', x1))[0]
            try:
                res = a_ ** b_
            except (OverflowError, ZeroDivisionError, ValueError):
                res = float('nan')
            uc.reg_write(UC_X86_REG_XMM0, struct.unpack('<Q', struct.pack('<d', float(res)))[0])
            return
        elif name == 'rand':
            self.stop_reason = 'port called rand()'
            uc.emu_stop()
        else:
            self.stop_reason = f'port called import {name}'
            uc.emu_stop()
        uc.reg_write(UC_X86_REG_EAX, r & 0xFFFFFFFF)

    def _logged_write(self, addr, data):
        if self.logging:
            self.wlog.append((addr, bytes(self.mu.mem_read(addr, len(data)))))
        self.mu.mem_write(addr, data)

    # ---- calls -----------------------------------------------------------------------------------
    def write(self, addr, data):
        """Write memory from the host (tools mirroring results): keeps the 64-bit port machine in sync."""
        self.mu.mem_write(addr, data)
        if self.mu64 is not None:
            for pg in range(addr >> 12, ((addr + len(data) - 1) >> 12) + 1):
                self.dirty.add(pg)

    def call(self, addr, args=(), max_insns=50_000_000, nested=False):
        """Call addr(args) (cdecl) until it returns. nested=True uses the lower half of the stack area
        (for calls made while an outer emulation, e.g. a tick, is paused). 64-bit builds: port functions run
        in the 64-bit machine (_call64); args and the result are game offsets either way."""
        if self.mu64 is not None and self.image_lo <= addr < self.image_hi:
            return self._call64(addr, args, max_insns, nested)
        mu = self.mu
        mu.context_restore(self.ctx)
        esp = STACK_BASE + (STACK_SIZE // 2 if nested else STACK_SIZE) - 0x1000
        frame = struct.pack('<I', TRAP) + b''.join(struct.pack('<I', x & 0xFFFFFFFF) for x in args)
        mu.mem_write(esp, frame)
        mu.reg_write(UC_X86_REG_ESP, esp)
        for r in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                  UC_X86_REG_EBP):
            mu.reg_write(r, 0)
        self.stop_reason = None
        try:
            mu.emu_start(addr, TRAP, count=max_insns)
        except UcError as e:
            self.stop_reason = self.stop_reason or f'{e} at eip {mu.reg_read(UC_X86_REG_EIP):#x}'
        if self.stop_reason is None and mu.reg_read(UC_X86_REG_EIP) != TRAP:
            self.stop_reason = f'did not return (eip {mu.reg_read(UC_X86_REG_EIP):#x})'
        return mu.reg_read(UC_X86_REG_EAX)

    def begin(self):
        self.wlog = []
        self.wlog64 = []
        self.logging = True

    def end(self):
        """Stop logging; return {addr: final byte} for every written byte (stack excluded)."""
        self.logging = False
        out = {}
        mu = self.mu
        for addr, old in self.wlog:
            if STACK_BASE <= addr < STACK_BASE + STACK_SIZE:
                continue
            for i in range(len(old)):
                out.setdefault(addr + i, None)
        if out:
            for a in list(out):
                out[a] = mu.mem_read(a, 1)[0]
        if self.mu64 is not None:
            out.update(self._end64())
        return out

    def undo(self):
        for addr, old in reversed(self.wlog):
            self.mu.mem_write(addr, old)
        self.wlog = []
        if self.mu64 is not None:
            for addr, old in reversed(self.wlog64):
                self.mu64.mem_write(addr, old)
            self.wlog64 = []

    # ---- 64-bit builds: the port's code in a 64-bit machine -------------------------------------------------
    # The 32-bit machine (self.mu) stays the reference memory in the game's offset space - the original runs
    # there, and the tools read and compare it. self.mu64 holds the same memory at the snapshotted process'
    # real addresses with the port's x64 code. Before every port call, pages written in self.mu are copied
    # over (real pointers held in port statics are kept as offsets in self.mu and converted on the way);
    # the port's writes are reported in offset space (_end64).
    def real_addr(self, off):
        """game offset -> real address of the snapshotted 64-bit process."""
        return off + (self.bases[0] if off < 0x20000000 else self.bases[1] if off < 0x40000000 else self.bases[2])

    def _init_port64(self):
        from unicorn import UC_MODE_64
        from unicorn.x86_const import UC_X86_REG_GS_BASE
        ps = self.ps
        self.mu64 = m64 = Uc(UC_ARCH_X86, UC_MODE_64)
        mapped = []
        for off, data in sorted(self.before):
            ra = self.real_addr(off)
            m64.mem_map(ra, len(data), UC_PROT_ALL)
            m64.mem_write(ra, data)
            mapped.append((ra, ra + len(data)))
        # code and constant data at the real image base, relocated (writable sections came with the dump)
        rb = self.real_addr(self.image_base)
        delta = rb - ps.image_base
        relocs = sorted(ps.reloc64_sites)
        for name, va, vsize, rawptr, rawsize in ps.sections:
            lo = rb + va
            size = (max(vsize, rawsize) + PAGE - 1) & ~(PAGE - 1)
            if any(not (hi <= lo or l >= lo + size) for l, hi in mapped):
                continue
            data = bytearray(ps.pe[rawptr:rawptr + min(rawsize, vsize)])
            data += bytes(size - len(data))
            i = bisect.bisect_left(relocs, va)
            while i < len(relocs) and relocs[i] < va + size:
                o = relocs[i] - va
                if o + 8 <= len(data):
                    struct.pack_into('<Q', data, o, (struct.unpack_from('<Q', data, o)[0] + delta) & ((1 << 64) - 1))
                i += 1
            m64.mem_map(lo, size, UC_PROT_ALL)
            m64.mem_write(lo, bytes(data))
            mapped.append((lo, lo + size))
        self.stub_funcs64 = {}
        self._bind_imports64(rb)
        m64.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_ALL)
        m64.mem_map(TRAP, PAGE, UC_PROT_ALL)
        m64.mem_write(TRAP, bytes([0xF4]) * 16)
        m64.mem_map(STUBS, 0x10000, UC_PROT_ALL)
        m64.mem_write(STUBS, bytes([0xC3]) * 0x10000)
        m64.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
        self.heap64 = HEAP_BASE
        # a minimal TEB for __chkstk (stack base/limit) and TLS reads: gs:[8] / gs:[0x10] / gs:[0x30] / gs:[0x58]
        teb = TEB64
        m64.mem_map(teb, 0x10000, UC_PROT_ALL)
        for o, v in ((8, STACK_BASE + STACK_SIZE), (0x10, STACK_BASE), (0x30, teb), (0x58, teb + 0x1000)):
            m64.mem_write(teb + o, struct.pack('<Q', v))
        m64.mem_write(teb + 0x1000, struct.pack('<Q', teb + 0x2000) * 64)
        m64.reg_write(UC_X86_REG_GS_BASE, teb)
        m64.hook_add(UC_HOOK_MEM_INVALID, self._on_invalid64)
        m64.hook_add(UC_HOOK_INTR, self._on_intr64)
        m64.hook_add(UC_HOOK_CODE, self._on_trap, begin=TRAP, end=TRAP + 16)
        m64.hook_add(UC_HOOK_CODE, self._on_stub64, begin=STUBS, end=STUBS + 0xFFFF)
        m64.hook_add(UC_HOOK_MEM_WRITE, self._on_write64)
        self.wlog64 = []
        self.ctx64 = m64.context_save()
        # real pointers held in port statics: offset form in the 32-bit machine (4-byte offset, 4 zero bytes)
        self.ptr_fields = sorted({self.image_base + s_.rva + o for s_ in ps.data_symbols() if s_.type_index and s_.size
                                  for o, w, real in ps.pointer_fields(s_.type_index) if real and w == 8})
        for p in self.ptr_fields:
            b = self.bdump.read(p, 8)
            if b is not None:
                v = struct.unpack('<Q', b)[0]
                self.mu.mem_write(p, struct.pack('<II', (self.real_to_off(v) or 0) if v else 0, 0))
        self._sigs = {}
        self._fn_by_rva = {s_.rva: s_ for s_ in ps.syms if s_.tag == SymTagFunction}

    def _bind_imports64(self, rb):
        ps, pe = self.ps, self.ps.pe
        imp_rva = struct.unpack_from('<I', pe, ps.data_dirs + 8)[0]
        p = ps.rva_to_off(imp_rva)
        k = 0
        while p is not None:
            oft, ts, fc, name_rva, ft = struct.unpack_from('<IIIII', pe, p)
            if name_rva == 0:
                break
            q = ps.rva_to_off(oft or ft)
            j = 0
            while True:
                v = struct.unpack_from('<Q', pe, q + 8 * j)[0]
                if v == 0:
                    break
                fname = f'ord{v & 0xFFFF}'
                if not v >> 63:
                    ho = ps.rva_to_off(v & 0x7FFFFFFF)
                    fname = pe[ho + 2:pe.index(bytes(1), ho + 2)].decode()
                stub = STUBS + 16 * k
                self.stub_funcs64[stub] = fname
                self.mu64.mem_write(rb + ft + 8 * j, struct.pack('<Q', stub))
                k += 1
                j += 1
            p += 20

    def _on_invalid64(self, uc, access, address, size, value, user):
        from unicorn.x86_const import UC_X86_REG_RIP
        self.stop_reason = f'port: invalid memory access (type {access}) at {address:#x}, rip {uc.reg_read(UC_X86_REG_RIP):#x}'
        return False

    def _on_intr64(self, uc, intno, user):
        from unicorn.x86_const import UC_X86_REG_RIP
        self.stop_reason = f'port: interrupt {intno:#x} at {uc.reg_read(UC_X86_REG_RIP):#x}'
        uc.emu_stop()

    def _on_write64(self, uc, access, address, size, value, user):
        if self.logging:
            self.wlog64.append((address, bytes(uc.mem_read(address, size))))

    def _logged_write64(self, addr, data):
        if self.logging:
            self.wlog64.append((addr, bytes(self.mu64.mem_read(addr, len(data)))))
        self.mu64.mem_write(addr, data)

    def _on_stub64(self, uc, address, size, user):
        from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RAX
        name = self.stub_funcs64.get(address, '?')
        a0, a1, a2 = (uc.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
        r = 0
        if name in ('memcpy', 'memmove'):
            if a2:
                self._logged_write64(a0, bytes(uc.mem_read(a1, a2)))
            r = a0
        elif name == 'memset':
            if a2:
                self._logged_write64(a0, bytes([a1 & 0xFF]) * a2)
            r = a0
        elif name in ('malloc', 'calloc'):
            n = a0 * (a1 if name == 'calloc' else 1)
            r = self.heap64
            self.heap64 += (n + 15) & ~15
        elif name in ('free', '__stdio_common_vfprintf', '__stdio_common_vsprintf', 'fflush', 'fputs', 'fputc',
                      '__acrt_iob_func', 'fwrite'):
            r = 0
        elif name == 'pow':
            from unicorn.x86_const import UC_X86_REG_XMM0, UC_X86_REG_XMM1
            x0 = uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)
            x1 = uc.reg_read(UC_X86_REG_XMM1) & ((1 << 64) - 1)
            a_, b_ = struct.unpack('<d', struct.pack('<Q', x0))[0], struct.unpack('<d', struct.pack('<Q', x1))[0]
            try:
                res = a_ ** b_
            except (OverflowError, ZeroDivisionError, ValueError):
                res = float('nan')
            uc.reg_write(UC_X86_REG_XMM0, struct.unpack('<Q', struct.pack('<d', float(res)))[0])
            return
        else:
            self.stop_reason = f'port called import {name}'
            uc.emu_stop()
        uc.reg_write(UC_X86_REG_RAX, r)

    def _sync64(self):
        """Copy the pages written in the 32-bit machine since the last port call into the 64-bit one."""
        if not self.dirty:
            return
        pages, self.dirty = sorted(self.dirty), set()
        for pg in pages:
            a = pg << 12
            if IDA_LO <= a < IDA_HI or a >= HEAP_BASE:  # the original's memory, harness stack/stubs/heap
                continue
            try:
                data = bytes(self.mu.mem_read(a, PAGE))
                self.mu64.mem_write(self.real_addr(a), data)
            except UcError:
                continue
            i = bisect.bisect_left(self.ptr_fields, a - 7)
            while i < len(self.ptr_fields) and self.ptr_fields[i] < a + PAGE:
                p = self.ptr_fields[i]
                i += 1
                try:
                    t = struct.unpack('<I', bytes(self.mu.mem_read(p, 4)))[0]
                    self.mu64.mem_write(self.real_addr(p), struct.pack('<Q', self.real_addr(t) if t else 0))
                except UcError:
                    pass

    def _signature(self, addr):
        if addr not in self._sigs:
            s_ = self._fn_by_rva.get(addr - self.image_base)
            self._sigs[addr] = self.ps.signature(s_) if s_ else None
        return self._sigs[addr]

    def _call64(self, addr, args, max_insns, nested):
        from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9
        from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RIP
        self._sync64()
        m64 = self.mu64
        m64.context_restore(self.ctx64)
        sig = self._signature(addr)
        vals = []
        for i, x in enumerate(args):
            x &= 0xFFFFFFFF
            ptr = sig is not None and i < len(sig[1]) and sig[1][i] == 'ptr'
            vals.append(self.real_addr(x) if ptr and x else x)
        # Windows x64: rcx, rdx, r8, r9, then the stack above 32 bytes of shadow space; rsp % 16 == 8 at entry
        rsp = STACK_BASE + (STACK_SIZE // 2 if nested else STACK_SIZE) - 0x1008
        m64.mem_write(rsp, struct.pack('<Q', TRAP) + bytes(32) + b''.join(struct.pack('<Q', v) for v in vals[4:]))
        m64.reg_write(UC_X86_REG_RSP, rsp)
        for reg, v in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), vals[:4]):
            m64.reg_write(reg, v)
        self.stop_reason = None
        try:
            m64.emu_start(self.real_addr(addr), TRAP, count=max_insns)
        except UcError as e:
            self.stop_reason = self.stop_reason or f'port: {e} at rip {m64.reg_read(UC_X86_REG_RIP):#x}'
        if self.stop_reason is None and m64.reg_read(UC_X86_REG_RIP) != TRAP:
            self.stop_reason = f'port did not return (rip {m64.reg_read(UC_X86_REG_RIP):#x})'
        rax = m64.reg_read(UC_X86_REG_RAX)
        if sig is not None and sig[0] == 'ptr':
            return (self.real_to_off(rax) or 0) if rax else 0
        return rax & 0xFFFFFFFF

    def _end64(self):
        """The port's writes since begin() in offset space ({offset: byte}); real pointers it stored in its
        statics in offset form (4-byte offset, 4 zero bytes - as the 32-bit machine holds them)."""
        offs = set()
        for addr, old in self.wlog64:
            for i in range(len(old)):
                o = self.real_to_off(addr + i)
                if o is not None:
                    offs.add(o)
        out = {}
        for o in offs:
            i = bisect.bisect_right(self.ptr_fields, o) - 1
            if i >= 0 and self.ptr_fields[i] <= o < self.ptr_fields[i] + 8:
                p = self.ptr_fields[i]
                v = struct.unpack('<Q', bytes(self.mu64.mem_read(self.real_addr(p), 8)))[0]
                out[o] = struct.pack('<II', (self.real_to_off(v) or 0) if v else 0, 0)[o - p]
            else:
                out[o] = self.mu64.mem_read(self.real_addr(o), 1)[0]
        return out

    def trace_calls(self, addr, args, nested=True):
        """Call addr(args) logging every direct call made at depth 1: [(target name, args, eax)]. Undone."""
        if self.mu64 is not None and self.image_lo <= addr < self.image_hi:
            return self._trace_calls64(addr, args, nested)
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        if not hasattr(self, '_fnames'):
            self._fnames = {self.image_base + s.rva: s.name for s in self.ps.syms if s.tag == SymTagFunction}
        out, pending = [], []
        start_esp = [None]

        def hook(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if start_esp[0] is None:
                start_esp[0] = esp
            if pending and address == pending[-1][0] and esp >= pending[-1][2]:
                out[pending[-1][1]][2] = uc.reg_read(UC_X86_REG_EAX)
                pending.pop()
            if pending:
                return
            ins = next(md.disasm(bytes(uc.mem_read(address, size)), address), None)
            if ins is None or ins.mnemonic != 'call':
                return
            tgt = ins.op_str
            try:
                t = int(tgt, 16)
                b = bytes(uc.mem_read(t, 5))
                if b[0] == 0xE9:
                    t = t + 5 + struct.unpack('<i', b[1:])[0]
                tgt = self._fnames.get(t, f'{t:#x}')
            except ValueError:
                pass
            a = struct.unpack('<4I', bytes(uc.mem_read(esp, 16)))
            out.append([tgt, a, None])
            pending.append((address + size, len(out) - 1, esp))

        h = self.mu.hook_add(UC_HOOK_CODE, hook)
        self.begin()
        self.call(addr, args, nested=nested)
        self.end()
        self.undo()
        self.mu.hook_del(h)
        return out

    def _trace_calls64(self, addr, args, nested):
        """trace_calls for port code on 64-bit builds: register arguments (rcx, rdx, r8, r9; real pointers
        shown as game offsets), return value from rax."""
        from capstone import Cs, CS_ARCH_X86, CS_MODE_64
        from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9
        from unicorn.x86_const import UC_X86_REG_RAX
        md = Cs(CS_ARCH_X86, CS_MODE_64)
        rb = self.real_addr(self.image_base)
        if not hasattr(self, '_fnames64'):
            self._fnames64 = {rb + s_.rva: s_.name for s_ in self.ps.syms if s_.tag == SymTagFunction}
        out, pending = [], []

        def hook(uc, address, size, user):
            rsp = uc.reg_read(UC_X86_REG_RSP)
            if pending and address == pending[-1][0] and rsp >= pending[-1][2]:
                out[pending[-1][1]][2] = uc.reg_read(UC_X86_REG_RAX) & 0xFFFFFFFF
                pending.pop()
            if pending:
                return
            ins = next(md.disasm(bytes(uc.mem_read(address, size)), address), None)
            if ins is None or ins.mnemonic != 'call':
                return
            tgt = ins.op_str
            try:
                t = int(tgt, 16)
                b_ = bytes(uc.mem_read(t, 5))
                if b_[0] == 0xE9:
                    t = t + 5 + struct.unpack('<i', b_[1:])[0]
                tgt = self._fnames64.get(t, f'{t:#x}')
            except ValueError:
                pass
            regs = [uc.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9)]
            out.append([tgt, tuple(self.real_to_off(v) if v > 0xFFFFFFFF and self.real_to_off(v) is not None
                                   else v & 0xFFFFFFFF for v in regs), None])
            pending.append((address + size, len(out) - 1, rsp))

        h = self.mu64.hook_add(UC_HOOK_CODE, hook, begin=rb, end=self.real_addr(self.image_hi))
        self.begin()
        self.call(addr, args, nested=nested)
        self.end()
        self.undo()
        self.mu64.hook_del(h)
        return out

    def port_addr(self, name):
        s = self.sym_by_name[name]
        return self.image_base + s.rva

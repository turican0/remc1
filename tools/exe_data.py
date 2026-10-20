#!/usr/bin/env python3
"""Read original initialised data from CARPET.EXE by IDA address.

The data object of the DOS4GW LE executable is stored uncompressed, so the bytes behind a global
such as dword_4A1FC_4A53C (first number = IDA address in *this* CD build) can be read directly.
Use it when a decompiled initializer looks truncated or wrong (ASan "global-buffer-overflow"
right after an initialised table is the typical symptom).

Usage:
  python tools/exe_data.py --exe <data>/CD/CARPET.EXE dump 0x4A1E4 0x18        # hex dump
  python tools/exe_data.py --exe <data>/CD/CARPET.EXE carray 0x4A17C 0x5A4 name  # C initializer
"""
import argparse
import struct
import sys

# CARPET.EXE is a DOS4GW-bound LE executable. IDA addresses are linear addresses inside its objects
# (object 0 = code+const data at 0x10000, object 2 = data at 0x90000, ...). File offset of an address
# = data_pages_offset + (first_page_of_object - 1 + page_in_object) * page_size + offset_in_page.
# Bytes past the stored pages of an object are BSS (zero).


class LeImage:
    def __init__(self, path):
        exe = open(path, "rb").read()
        off = exe.find(b"LE\x00\x00")
        if off < 0:
            sys.exit("no LE header in " + path)
        self.exe = exe
        self.page_size = struct.unpack_from("<I", exe, off + 0x28)[0]
        objtab = struct.unpack_from("<I", exe, off + 0x40)[0]
        nobj = struct.unpack_from("<I", exe, off + 0x44)[0]
        self.data_pages = struct.unpack_from("<I", exe, off + 0x80)[0]
        self.objects = []
        for i in range(nobj):
            vsize, base, flags, pidx, pcnt, _ = struct.unpack_from("<6I", exe, off + objtab + i * 24)
            self.objects.append((base, vsize, pidx, pcnt))

    def read(self, addr, length):
        out = bytearray()
        for a in range(addr, addr + length):
            for base, vsize, pidx, pcnt in self.objects:
                if base <= a < base + vsize:
                    rel = a - base
                    page = rel // self.page_size
                    if page < pcnt:
                        out.append(self.exe[self.data_pages + (pidx - 1 + page) * self.page_size + rel % self.page_size])
                    else:
                        out.append(0)  # BSS
                    break
            else:
                sys.exit(f"address {a:#x} is outside every LE object")
        return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("cmd", choices=["dump", "carray", "strptrs", "disasm"])
    ap.add_argument("addr", type=lambda x: int(x, 0))
    ap.add_argument("length", type=lambda x: int(x, 0), help="bytes (dump/carray) or entries (strptrs)")
    ap.add_argument("name", nargs="?", default="data")
    ap.add_argument("--base", type=lambda x: int(x, 0), default=0x90000,
                    help="strptrs: object base the stored pointers are relative to (LE fixups are not "
                         "applied in the file; data-object pointers are offsets from 0x90000)")
    a = ap.parse_args()
    img = LeImage(a.exe)
    if a.cmd == "strptrs":
        # A table of char* in the data object -> C initializer with the strings themselves.
        print(f"char* {a.name}[{a.length}] = {{ // CARPET.EXE {a.addr:#x}, {a.length} string pointers")
        for i in range(a.length):
            p = struct.unpack_from("<I", img.read(a.addr + 4 * i, 4))[0]
            if p == 0:
                print("    nullptr,")
                continue
            s, q = bytearray(), a.base + p
            while (c := img.read(q, 1)) != b"\0":
                s += c
                q += 1
            esc = "".join(ch if 32 <= ord(ch) < 127 and ch not in '"\\' else f"\\x{ord(ch):02x}" if ord(ch) < 32 or ord(ch) >= 127 else "\\" + ch
                          for ch in s.decode("latin-1"))
            print(f'    (char*)"{esc}",')
        print("};")
        return
    b = img.read(a.addr, a.length)
    if a.cmd == "disasm":
        # The ground truth when a decompiled line looks wrong (pip install capstone).
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        for ins in md.disasm(b, a.addr):
            print(f"{ins.address:06X}: {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}")
        return
    if a.cmd == "dump":
        for i in range(0, len(b), 16):
            print(f"{a.addr + i:06X}: " + " ".join(f"{x:02x}" for x in b[i:i + 16]))
    else:
        print(f"uint8_t {a.name}[0x{a.length:X}] = {{ // CARPET.EXE {a.addr:#x}..{a.addr + a.length:#x}")
        for i in range(0, len(b), 16):
            print("    " + ", ".join(f"0x{x:02X}" for x in b[i:i + 16]) + ",")
        print("};")


if __name__ == "__main__":
    main()

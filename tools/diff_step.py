#!/usr/bin/env python3
"""Stepwise differential test inside a recorded call: stop the ORIGINAL code at every entry of the
given inner functions, run the original and the port version of that function on the same memory,
compare their writes, keep the original's result and go on.

    python tools/diff_step.py SNAPDIR 0x2DCB0 [0x729A3 ...] [--nargs 1] [--show 8]

SNAPDIR is a "--snapshot-what draw" (or tick) snapshot. For a tick snapshot the outer call is
UpdateEntities (0x41780). Finds which call of a renderer helper first draws differently (e.g. one sprite).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
from emu import Machine, STACK_BASE, STACK_SIZE, TRAP
from diff_funcs import compare, port_name_for


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snapdir')
    ap.add_argument('funcs', nargs='+')
    ap.add_argument('--nargs', type=int, default=1, help='stack arguments of the inner functions')
    ap.add_argument('--show', type=int, default=8)
    a = ap.parse_args()
    m = Machine(a.snapdir)
    mu = m.mu
    if 'call' in m.meta:
        ida, args = m.meta['call'][0], list(m.meta['call'][1:])
        if ida == 0x30D90:  # the original DrawWorld takes the screen pointer as a hidden first argument
            args = [struct.unpack('<I', bytes(mu.mem_read(0x12EFF4, 4)))[0]] + args
    else:
        ida, args = 0x41780, []
    targets = {int(f, 16): port_name_for(m, int(f, 16)) for f in a.funcs}
    stopped = {}

    def hook(uc, address, size, user):
        stopped['at'] = address
        uc.emu_stop()

    hooks = [mu.hook_add(UC_HOOK_CODE, hook, begin=t, end=t) for t in targets]
    mu.context_restore(m.ctx)
    esp = STACK_BASE + STACK_SIZE - 0x1000
    mu.mem_write(esp, struct.pack('<I', TRAP) + b''.join(struct.pack('<I', x & 0xFFFFFFFF) for x in args))
    mu.reg_write(UC_X86_REG_ESP, esp)
    pc = ida
    steps, shown = {}, 0
    def mirror(written):
        # original writes to original-addressed globals -> the port's globals, so the port function sees
        # the same world (the caller's own writes between steps as well as the kept results)
        for addr, val in written.items():
            if 0x10000 <= addr < 0x150000:
                pa, _ = m.ida_to_port(addr)
                if pa is not None:
                    m.write(pa, bytes([val]))

    while True:
        stopped.clear()
        m.begin()
        mu.emu_start(pc, TRAP, count=2_000_000_000)
        outer = m.end()
        m.wlog = []
        if not stopped:
            break
        t = stopped['at']
        ctx = mu.context_save()
        mirror(outer)
        sp = mu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', bytes(mu.mem_read(sp, 4)))[0]
        fargs = list(struct.unpack(f'<{a.nargs}I', bytes(mu.mem_read(sp + 4, 4 * a.nargs))))
        steps[t] = steps.get(t, 0) + 1
        for h in hooks:
            mu.hook_del(h)
        pname = targets[t]
        m.begin(); r_o = m.call(t, fargs, nested=True); e_o = m.stop_reason; w_o = m.end(); m.undo()
        m.begin(); r_p = m.call(m.port_addr(pname), fargs, nested=True); e_p = m.stop_reason; w_p = m.end(); m.undo()
        d, _ = compare(m, w_o, r_o, w_p, r_p, 6) if not (e_o or e_p) else ([f'error: original {e_o}, port {e_p}'], None)
        d = [x for x in d if not x.startswith('return value')]
        if d and shown < a.show:
            shown += 1
            print(f'{pname} call {steps[t]} args {[hex(x) for x in fargs]}:')
            for x in d:
                print('    ' + x)
        # keep the original's result (mirrored into port globals), then return to the caller
        m.begin(); r = m.call(t, fargs, nested=True); kept = m.end()
        m.wlog = []
        mirror(kept)
        mu.context_restore(ctx)
        mu.reg_write(UC_X86_REG_ESP, sp + 4)
        mu.reg_write(UC_X86_REG_EAX, (r or 0) & 0xFFFFFFFF)
        pc = ret
        hooks = [mu.hook_add(UC_HOOK_CODE, hook, begin=x, end=x) for x in targets]
    for h in hooks:
        mu.hook_del(h)
    print(f'# {sum(steps.values())} calls stepped ({", ".join(f"{targets[t]} x{n}" for t, n in steps.items())}), {shown} differed')


if __name__ == '__main__':
    main()

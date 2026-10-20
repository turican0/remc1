#!/usr/bin/env python3
"""Whole-tick differential test: the ORIGINAL UpdateEntities (0x41780) on the port's memory snapshot vs.
the port's own result of the same tick.

1. remc1.exe ... --snapshot-frame N --snapshot-dir D   (D/before.bin, D/after.bin, D/meta.txt)
2. python tools/diff_tick.py D

Differences are listed per entity (class/model/state, field offsets). Narrow them down per handler
with tools/diff_funcs.py (runs original and port code side by side on the same state).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from emu import Machine

ENTITY_BASE, ENTITY_SIZE, ENTITY_COUNT = 29795, 164, 1000


DISPATCH = 0x41A71   # call dword ptr [eax+6] in UpdateEntities (push ebx = entity just before)
AFTER_DISPATCH = 0x41A74


def stepwise(m, a):
    """Run the original tick; at every handler dispatch compare original vs port handler on the
    current memory, keep the original's result and continue."""
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
    from emu import STACK_BASE, STACK_SIZE, TRAP
    from diff_funcs import compare, port_name_for
    import emu as emumod
    mu = m.mu
    stopped = {}

    def hook(uc, address, size, user):
        stopped['at'] = True
        uc.emu_stop()

    h = mu.hook_add(UC_HOOK_CODE, hook, begin=DISPATCH, end=DISPATCH)
    names = {}
    mu.context_restore(m.ctx)
    esp = STACK_BASE + STACK_SIZE - 0x1000
    mu.mem_write(esp, struct.pack('<I', TRAP))
    mu.reg_write(UC_X86_REG_ESP, esp)
    pc = 0x41780
    st = m.state_addr
    shown = steps = 0
    while True:
        stopped.clear()
        mu.emu_start(pc, TRAP, count=500_000_000)
        if not stopped:
            break
        ctx = mu.context_save()
        ent = mu.reg_read(UC_X86_REG_EBX)
        tbl = mu.reg_read(UC_X86_REG_EAX)
        handler = struct.unpack('<I', bytes(mu.mem_read(tbl + 6, 4)))[0]
        idx = (ent - st - ENTITY_BASE) // ENTITY_SIZE
        if handler not in names:
            names[handler] = port_name_for(m, handler)
        pname = names[handler]
        steps += 1
        if pname:
            m.begin(); r_o = m.call(handler, [ent], nested=True); e_o = m.stop_reason; w_o = m.end(); m.undo()
            m.begin(); r_p = m.call(m.port_addr(pname), [ent], nested=True); e_p = m.stop_reason; w_p = m.end(); m.undo()
            d, _ = compare(m, w_o, r_o, w_p, r_p, 6) if not (e_o or e_p) else ([f'error orig {e_o} port {e_p}'], None)
            if d and shown < a.show:
                shown += 1
                eb = bytes(mu.mem_read(ent, 71))
                print(f'step {steps}: entity {idx} (class {eb[64]} model {eb[65]} state {eb[70]}) {pname}:')
                for x in d:
                    print('    ' + x)
            if steps == a.trace_step:
                for label, fa in (('original', handler), ('port', m.port_addr(pname))):
                    print(f'  calls ({label}):')
                    for t, ar, r in m.trace_calls(fa, [ent]):
                        print(f'    {t}({", ".join(hex(x) for x in ar)}) -> {r if r is None else hex(r)}')
        # keep the original's result - and mirror its writes to original-addressed globals into the
        # port's globals, so the next port call sees the same world (map cell lists, ...)
        m.begin()
        m.call(handler, [ent], nested=True)
        kept = m.end()
        m.wlog = []
        for addr, val in kept.items():
            if 0x10000 <= addr < 0x150000:
                pa, _ = m.ida_to_port(addr)
                if pa is not None:
                    m.write(pa, bytes([val]))
        mu.context_restore(ctx)
        pc = AFTER_DISPATCH
    mu.hook_del(h)
    print(f'# {steps} handler calls in the tick, {shown} differed')
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('snapdir')
    ap.add_argument('--show', type=int, default=60)
    ap.add_argument('--stepwise', action='store_true', help='compare every handler call of the tick in order')
    ap.add_argument('--trace-step', type=int, default=-1, help='with --stepwise: log the calls of both versions at step N')
    ap.add_argument('--watch', default='', help='comma separated game-state offsets: print who writes them (original)')
    a = ap.parse_args()
    m = Machine(a.snapdir, load_port_code=a.stepwise)
    m.watch = {m.state_addr + int(w, 0) for w in a.watch.split(',') if w}
    if a.stepwise:
        return stepwise(m, a)
    st, ss = m.state_addr, m.state_size
    org = bytes(m.mu.mem_read(st, ss))
    m.call(0x41780, [], max_insns=500_000_000)
    if m.stop_reason:
        print('STOPPED:', m.stop_reason)
        return 2
    emu = bytes(m.mu.mem_read(st, ss))
    prt = m.after.read(st, ss)
    diffs = []
    for i in range(ss):
        if emu[i] == prt[i]:
            continue
        # pointer written as an original address by the original and as a port address by the port
        ok = False
        for s in range(i - 3, i + 1):
            if 0 <= s and s + 4 <= ss:
                ov, pv = struct.unpack_from('<I', emu, s)[0], struct.unpack_from('<I', prt, s)[0]
                if ov != pv and m.translate_value(pv) == ov:
                    ok = True
                    break
        if not ok:
            diffs.append(i)
    print(f'game state: {len(diffs)} bytes differ between original and port '
          f'(original changed {sum(1 for i in range(ss) if emu[i] != org[i])}, port changed {sum(1 for i in range(ss) if prt[i] != org[i])})')
    by_entity, other = {}, []
    for i in diffs:
        if ENTITY_BASE <= i < ENTITY_BASE + ENTITY_SIZE * ENTITY_COUNT:
            e, f = divmod(i - ENTITY_BASE, ENTITY_SIZE)
            by_entity.setdefault(e, []).append(f)
        else:
            other.append(i)
    for e, fields in sorted(by_entity.items())[:a.show]:
        eb = ENTITY_BASE + ENTITY_SIZE * e
        fs = sorted(set(f & ~1 for f in fields))
        desc = ', '.join(f'+{f}: before {struct.unpack_from("<H", org, eb + f)[0]} orig {struct.unpack_from("<H", emu, eb + f)[0]} '
                         f'port {struct.unpack_from("<H", prt, eb + f)[0]}' for f in fs[:6])
        print(f'  entity {e} (class {org[eb + 64]} model {org[eb + 65]} state {org[eb + 70]}): {desc}{" ..." if len(fs) > 6 else ""}')
    if other:
        print('  outside the entity array: ' + ', '.join(f'+{i}: orig {emu[i]} port {prt[i]}' for i in other[:30]))
    return 0


if __name__ == '__main__':
    sys.exit(main())

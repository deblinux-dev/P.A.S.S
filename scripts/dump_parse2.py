#!/usr/bin/env python3
"""Минидамп-парсер v2: контекст + сырой бэктрейс стека + поиск строк."""
import struct, sys, re

def rdq(buf, off): return struct.unpack_from('<Q', buf, off)[0]

def parse(path, stack_dump=200, strhunt=True):
    buf = open(path, 'rb').read()
    sig, ver, nstreams, rva, chk, ts, flags = struct.unpack_from('<IIIIIIQ', buf, 0)
    streams = {}
    for i in range(nstreams):
        t, sz, s = struct.unpack_from('<III', buf, rva + i*12)
        streams.setdefault(t, []).append((sz, s))

    mods = []
    n = struct.unpack_from('<I', buf, streams[4][0][1])[0]
    off = streams[4][0][1]
    for i in range(n):
        mo = off + 4 + i*108
        base, msize, chk, ts, namerva = struct.unpack_from('<QIIII', buf, mo)
        ln = struct.unpack_from('<I', buf, namerva)[0]
        name = buf[namerva+4:namerva+4+ln].decode('utf-16-le', 'replace')
        mods.append((base, msize, name.split('\\')[-1]))
    for base, msize, name in mods: print(f'  MOD {base:012x}+{msize:8x} {name}')

    def mod_of(a):
        for base, msize, name in mods:
            if base <= a < base+msize: return f'{name}+0x{a-base:x}'
        return None

    # memory ranges
    ranges = []
    if 9 in streams:
        sz, off = streams[9][0]
        n, base_rva = struct.unpack_from('<QQ', buf, off)
        rva2 = base_rva
        for i in range(n):
            sa, dsz = struct.unpack_from('<QQ', buf, off+16+i*16)
            ranges.append([sa, dsz, rva2]); rva2 += dsz
    elif 5 in streams:
        sz, off = streams[5][0]
        n = struct.unpack_from('<I', buf, off)[0]
        for i in range(n):
            sa, dsz, mrva = struct.unpack_from('<QII', buf, off+4+i*16)
            ranges.append([sa, dsz, mrva])

    def mem(addr, ln):
        for sa, dsz, r in ranges:
            if sa <= addr and addr+ln <= sa+dsz:
                return buf[r+(addr-sa):r+(addr-sa)+ln]
        return None

    # context
    ctx_sz, ctx_rva = 0, 0
    if 6 in streams:
        sz, off = streams[6][0]
        tid = struct.unpack_from('<I', buf, off)[0]
        exr = off + 8
        code = struct.unpack_from('<I', buf, exr)[0]
        info0 = struct.unpack_from('<Q', buf, exr+32)[0]
        ctx_sz, ctx_rva = struct.unpack_from('<II', buf, exr+152)
        print(f'  EXC tid={tid} code=0x{code:08x} info[0]=0x{info0:x}')
    offs = {'Rip':0xF8,'Rsp':0x98,'Rbp':0xA0,'Rax':0x78,'Rcx':0x80,'Rdx':0x88,'Rbx':0x90,
            'Rsi':0xA8,'Rdi':0xB0,'R8':0xB8,'R9':0xC0,'R10':0xC8,'R11':0xD0,'R12':0xD8,
            'R13':0xE0,'R14':0xE8,'R15':0xF0}
    regs = {k: rdq(buf, ctx_rva+v) for k, v in offs.items()}
    for k in ['Rip','Rsp','Rbp','Rax','Rbx','Rcx','Rdx','Rsi','Rdi','R8','R9','R10','R11','R12','R13','R14','R15']:
        m = mod_of(regs[k])
        print(f'  {k}=0x{regs[k]:x} {m or ""}')

    rsp = regs['Rsp']
    print(f'  --- stack scan from rsp (0x{rsp:x}), {stack_dump} qwords ---')
    stk = mem(rsp, stack_dump*8)
    if stk:
        for i in range(0, len(stk), 8):
            v = struct.unpack_from('<Q', stk, i)[0]
            tag = mod_of(v)
            extra = ''
            if tag: extra = f'  <== {tag}'
            else:
                s = mem(v, 48)
                if s and all(32 <= c < 127 or c in (0,) for c in s[:12]):
                    t2 = s.split(b'\0')[0]
                    if len(t2) >= 6: extra = f'  ascii: {t2[:40]!r}'
            if tag or extra or i < 96:
                print(f'  rsp+{i:04x}: 0x{v:016x}{extra}')
    if strhunt:
        print('  --- printable strings near top of stack ---')
        s = mem(rsp, 4096)
        if s:
            for m2 in re.finditer(rb'[\x20-\x7e]{8,}', s[:2048]):
                print(f'    +0x{m2.start():04x}: {m2.group()[:60]!r}')

if __name__ == '__main__':
    parse(sys.argv[1])

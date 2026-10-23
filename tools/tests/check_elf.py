#!/usr/bin/env python3
"""Validate the linked BRAHMA ELF structure (not equivalent to a boot test)."""
import struct
import sys
from pathlib import Path

p = Path(sys.argv[1] if len(sys.argv) > 1 else 'build/brahma.elf')
data = p.read_bytes()
assert data[:4] == b'\x7fELF', 'not an ELF'
assert data[4:6] == b'\x02\x01', 'expected 64-bit little endian'
(e_type, e_machine) = struct.unpack_from('<HH', data, 16)
assert (e_type, e_machine) == (2, 62), 'expected ET_EXEC x86-64'
entry, phoff, shoff = struct.unpack_from('<QQQ', data, 24)
phentsize, phnum = struct.unpack_from('<HH', data, 54)
assert 0xFFFFFFFF80000000 <= entry < 0xFFFFFFFFC0000000, 'kernel entry not higher half'
load_segments = []
for i in range(phnum):
    offset = phoff + i * phentsize
    ptype, flags, file_off, vaddr, paddr, file_size, mem_size, align = struct.unpack_from('<IIQQQQQQ', data, offset)
    assert ptype not in (2, 3), 'must not have DYNAMIC or INTERP program headers'
    if ptype == 1:
        assert mem_size >= file_size
        assert not (flags & 0x1 and flags & 0x2), 'W^X violated'
        assert vaddr >= 0xFFFFFFFF80000000
        assert align in (0x1000, 0x200000)
        load_segments.append((vaddr, vaddr + mem_size, flags))
assert len(load_segments) == 4, f'expected 4 PT_LOAD segments, got {len(load_segments)}'
assert any(start <= entry < end and flags & 1 for start, end, flags in load_segments)
assert data.count(struct.pack('<Q', 0xf6b8f4b39de7d1ae)) >= 1, 'Limine start marker missing'
assert data.count(struct.pack('<Q', 0xadc0e0531bb10d03)) >= 1, 'Limine end marker missing'
assert data.count(struct.pack('<Q', 0xf9562b2d5c95a6c8)) >= 1, 'Limine base revision missing'
print(f'[ELF][PASS] {p}: ELF64 x86-64 higher-half, W^X, Limine request markers')

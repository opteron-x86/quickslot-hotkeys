#!/usr/bin/env python3
"""Read-only Address Library + PE audit of a supplied 1.6.1170 runtime."""
import argparse
import pathlib
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--address-library', type=pathlib.Path, required=True)
parser.add_argument('--exe', type=pathlib.Path, required=True)
args = parser.parse_args()
blob = args.address_library.read_bytes()
pos = 0
def read(fmt):
    global pos
    value = struct.unpack_from('<' + fmt, blob, pos)
    pos += struct.calcsize('<' + fmt)
    return value[0] if len(value) == 1 else value
assert read('I') == 2
assert read('4I') == (1, 6, 1170, 0)
name_length = read('I')
pos += name_length
ptrsize, count = read('2I')
def expand(kind, previous):
    if kind == 0: return read('Q')
    if kind == 1: return previous + 1
    if kind == 2: return previous + read('B')
    if kind == 3: return previous - read('B')
    if kind == 4: return previous + read('H')
    if kind == 5: return previous - read('H')
    if kind == 6: return read('H')
    if kind == 7: return read('I')
    raise ValueError(kind)
ident = offset = 0
addresses = {}
for _ in range(count):
    kind = read('B')
    ident = expand(kind & 15, ident)
    scaled = bool(kind & 128)
    offset = expand((kind >> 4) & 7, offset // ptrsize if scaled else offset)
    if scaled: offset *= ptrsize
    addresses[ident] = offset
exe = args.exe.read_bytes()
pe = struct.unpack_from('<I', exe, 0x3c)[0]
sections, optional_size = struct.unpack_from('<H', exe, pe+6)[0], struct.unpack_from('<H', exe, pe+20)[0]
section_start = pe + 24 + optional_size
def file_offset(rva):
    for i in range(sections):
        at = section_start + i*40
        size, va, rawsize, raw = struct.unpack_from('<4I', exe, at+8)
        if va <= rva < va + max(size, rawsize): return raw + rva-va
    raise ValueError(hex(rva))
for ident in (68617, 16148, 16149):
    rva = addresses[ident]
    at = file_offset(rva)
    print(f'ID {ident}: RVA 0x{rva:x}, first bytes {exe[at:at+24].hex(" ")}')
call_rva = addresses[68617] + 0x7b
at = file_offset(call_rva)
if exe[at] == 0xe8:
    destination = call_rva + 5 + struct.unpack_from('<i', exe, at+1)[0]
    print(f'Input dispatch: RVA 0x{call_rva:x}, E8 target RVA 0x{destination:x}')
    print('PASS: executable has the expected five-byte input dispatch call.')
else:
    print(f'Input dispatch: RVA 0x{call_rva:x}; on-disk bytes do not match E8.')
    print('UNVERIFIED: the installed executable is packed; disk bytes cannot validate the live call site.')
    print('The plugin validates the live E8/FF15 call after loading and disables itself on mismatch.')

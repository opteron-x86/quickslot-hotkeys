#!/usr/bin/env python3
"""Check direct relocation IDs against supplied SE, AE and VR address tables."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
SITES = [
    ('ExtraDataList constructor', 11437, 11583, True),
    ('Player inventory damage', 39179, 40253, True),
    ('Player inventory armor rating', 39175, 40249, True),
    ('Input polling', 67315, 68617, True),
    ('Current time multiplier', 511882, 388442, True),
    ('Target time multiplier', 511883, 388443, True),
    ('Native spell unequip', 37947, 38903, False),
    ('Native shout unequip', 37948, 38904, False),
]


def ae_binary(path):
    blob = path.read_bytes()
    pos = 0

    def read(fmt):
        nonlocal pos
        values = struct.unpack_from('<' + fmt, blob, pos)
        pos += struct.calcsize('<' + fmt)
        return values[0] if len(values) == 1 else values

    assert read('I') == 2, f'Expected AE format 2: {path}'
    version = '.'.join(map(str, read('4I')))
    name_size = read('I')
    pos += name_size
    pointer_size, count = read('2I')
    assert pointer_size == 8

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
    result = {}
    for _ in range(count):
        kind = read('B')
        ident = expand(kind & 15, ident)
        scaled = bool(kind & 128)
        offset = expand((kind >> 4) & 7, offset // pointer_size if scaled else offset)
        if scaled: offset *= pointer_size
        result[ident] = offset
    assert pos == len(blob), f'Trailing data: {path}'
    return version, result


def csv_table(path, ident, address):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return {int(row[ident]): int(row[address], 16) for row in csv.DictReader(stream)
                if row[ident] and row[address]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--se-csv', type=Path, required=True)
    parser.add_argument('--vr-csv', type=Path, required=True)
    parser.add_argument('--ae17-csv', type=Path, required=True)
    parser.add_argument('--ae-directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Fail when a new direct relocation is added without an audit entry.
    found = set()
    for path in (ROOT / 'src').glob('*'):
        if path.suffix not in {'.h', '.cpp'}: continue
        text = re.sub(r'//[^\n]*|/\*.*?\*/', '', path.read_text(encoding='utf-8'), flags=re.S)
        found.update(tuple(map(int, pair)) for pair in re.findall(r'RELOCATION_ID\(\s*(\d+)\s*,\s*(\d+)\s*\)', text))
    # Also check the engine wrappers used to score smart weapons and shields.
    player_source = (ROOT / 'vendor/commonlib/src/RE/P/PlayerCharacter.cpp').read_text(encoding='utf-8')
    for name in ('GetDamage', 'GetArmorValue'):
        wrapper = re.search(r'float PlayerCharacter::' + name + r'\(.*?RELOCATION_ID\(\s*(\d+)\s*,\s*(\d+)\s*\)', player_source, re.S)
        assert wrapper, f'Pinned {name} wrapper was not found'
        found.add((int(wrapper[1]), int(wrapper[2])))
    assert found == {(se, ae) for _, se, ae, _ in SITES}, (found, SITES)
    tables = [
        ('1.5.97', args.se_csv, csv_table(args.se_csv, 'sseid', 'sse_addr'), 'se'),
        ('1.7.104', args.ae17_csv, csv_table(args.ae17_csv, 'aeid', 'ae_addr'), 'ae'),
        ('VR 1.4.15', args.vr_csv, csv_table(args.vr_csv, 'id', 'vr'), 'vr'),
    ]
    binaries = sorted(args.ae_directory.glob('versionlib-1-6-*.bin'))
    assert binaries, 'No AE address databases supplied'
    for path in binaries:
        version, addresses = ae_binary(path)
        tables.append((version, path, addresses, 'ae'))
    records = []
    for version, path, addresses, family in tables:
        checked = []
        for label, se, ae, direct_vr in SITES:
            ident = ae if family == 'ae' else se
            if family == 'vr' and not direct_vr:
                checked.append({'function': label, 'route': 'registered Actor VM native; direct ID unused'})
                continue
            assert addresses.get(ident, 0) != 0, f'{version}: missing {label} ID {ident}'
            checked.append({'function': label, 'id': ident, 'address': hex(addresses[ident])})
        records.append({'version': version, 'table': path.name,
                        'table_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'sites': checked})
    report = {'passed': True, 'scope': 'Direct numeric relocations and player damage/armor wrappers',
              'databases': records}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: direct relocation availability in {len(records)} supplied databases; VR unequip uses VM natives')


if __name__ == '__main__':
    main()

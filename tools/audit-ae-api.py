#!/usr/bin/env python3
"""Check compiled object references for unsafe CommonLib engine wrappers."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--commonlib', type=Path, required=True)
parser.add_argument('objects', type=Path, nargs='+')
args = parser.parse_args()
missing = {}
pattern = re.compile(r'using\s+func_t\s*=\s*decltype\(&([\w:]+)\);\s*'
                     r'(?:static\s+)?REL::Relocation<func_t>\s+\w+\s*\{\s*RELOCATION_ID\(\s*\d+\s*,\s*0\s*\)')
for folder in ('src/RE', 'include/RE'):
    for path in sorted((args.commonlib / folder).rglob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        source = path.read_text(encoding='utf-8')
        for match in pattern.finditer(source):
            name = match[1]
            if '::' in name:
                missing['RE::' + name + '('] = f'{path}:{source[:match.start()].count(chr(10)) + 1}'
assert 'RE::ExtraDataList::GetObjectHealth(' in missing, 'Expected missing-AE-ID wrapper was not discovered'
objects = []
for path in args.objects:
    objects.extend(sorted(path.rglob('*.obj')) if path.is_dir() else [path])
assert objects and all(path.is_file() for path in objects), 'Expected compiled plugin objects'
symbols = subprocess.check_output(['llvm-nm', '--undefined-only', '--demangle', *map(str, objects)], text=True)
unsafe = [(name, source) for name, source in missing.items() if name in symbols]
for name, source in unsafe:
    print(f'FAIL: compiled plugin calls {name[:-1]} with a zero AE relocation: {source}')
bad_ammo = 'RE::Actor::GetCurrentAmmo(' in symbols
if bad_ammo:
    print('FAIL: compiled plugin calls Actor::GetCurrentAmmo, whose pinned wrapper dispatches GetDecalGroup. Use TESObjectREFR::GetCurrentAmmo.')
if unsafe or bad_ammo:
    raise SystemExit(1)
print(f'PASS: {len(objects)} compiled objects checked against {len(missing)} known missing-AE-ID wrappers and the invalid actor ammo wrapper.')

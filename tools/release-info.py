#!/usr/bin/env python3
"""Check the release version and pinned dependencies; emit CI metadata."""
import argparse
import json
import os
import re
from release_support import ROOT, package_version, release_notes, source_state, verify_dependencies, version

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--github-output', action='store_true')
args = parser.parse_args()
release = version()
base = release.split('-')[0]
cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
if not re.search(r'project\(Quickslot VERSION ' + re.escape(base) + r'\b', cmake):
    raise SystemExit('CMake project version differs from src/Version.h')
if json.loads((ROOT / 'vcpkg.json').read_text(encoding='utf-8'))['version-semver'] != base:
    raise SystemExit('vcpkg version differs from the base release version')
state = source_state()
tag = os.environ.get('GITHUB_REF_NAME') if os.environ.get('GITHUB_REF_TYPE') == 'tag' else None
if tag and state['dirty']:
    raise SystemExit('Tagged releases require a clean source tree')
label = package_version(release, state['commit'], tag)
release_notes(release)
count = verify_dependencies()
values = {'version': release, 'package_version': label, 'commit': state['commit'],
          'prerelease': str('-' in release).lower()}
print(f'Verified {count} dependency files; package {label}')
if args.github_output:
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
        for key, value in values.items():
            stream.write(f'{key}={value}\n')

#!/usr/bin/env python3
"""Package a tested DLL, matching source, symbols and API."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

from release_support import ROOT, digest, package_version, release_notes, source_files, source_state, verify_dependencies, version


def validate_runtime(files):
    required = {'SKSE/Plugins/Quickslot.dll', 'SKSE/Plugins/Quickslot.ini',
                'Quickslot/README.md'}
    if not required <= set(files):
        raise ValueError(f'Missing runtime files: {required - set(files)}')
    if {n for n in files if n.startswith('SKSE/')} != {'SKSE/Plugins/Quickslot.dll', 'SKSE/Plugins/Quickslot.ini'}:
        raise ValueError('Unexpected SKSE files in runtime package')
    for name in files:
        path = Path(name)
        if not name.startswith(('SKSE/', 'Quickslot/')) or '..' in path.parts:
            raise ValueError(f'Unexpected package path: {name}')
        if name.startswith('Quickslot/icons/'):
            raise ValueError(f'Unexpected item icon asset: {name}')
        if any(p.lower() in {'presets', 'source', 'backups', '.git'} for p in path.parts):
            raise ValueError(f'Unexpected runtime directory: {name}')
        if path.suffix.lower() in {'.pdb', '.exe', '.log', '.ess', '.skse', '.swf'}:
            raise ValueError(f'Unexpected runtime file: {name}')
        if path.name.lower() == 'settings.json' or 'controlmap' in name.lower():
            raise ValueError(f'User configuration cannot be packaged: {name}')
        if path.suffix.lower() == '.dll' and name != 'SKSE/Plugins/Quickslot.dll':
            raise ValueError(f'Unexpected DLL: {name}')


def validate_tests(report, dll, state, release):
    expected = {'plugin', 'runtime_vr', 'smart_equipment', 'shortcut_library', 'interactions',
                'localization', 'dx_state', 'controller_voice', 'icon', 'live', 'features',
                'hand_actions', 'advanced', 'consumable', 'smf', 'pngii'}
    if report.get('passed') is not True or set(report.get('cases', {})) != expected:
        raise ValueError('All Windows probes must pass before packaging')
    if any(v.get('returncode') != 0 for v in report['cases'].values()):
        raise ValueError('Windows probe failure in validation report')
    if report.get('dll_sha256') != digest(dll):
        raise ValueError('DLL differs from the tested binary')
    if report.get('source') != state or report.get('version') != release:
        raise ValueError('Validation belongs to a different source revision or release')


def archive(path, files, epoch):
    if path.exists():
        raise ValueError(f'Refusing to overwrite {path}')
    date = datetime.datetime.fromtimestamp(max(epoch, 315532800), datetime.timezone.utc)
    with zipfile.ZipFile(path, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for name, file in sorted(files.items()):
            info = zipfile.ZipInfo(name, date.timetuple()[:6])
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if name.endswith('.sh') else 0o100644) << 16
            zipped.writestr(info, file.read_bytes())
    with zipfile.ZipFile(path) as zipped:
        if zipped.testzip() is not None or set(zipped.namelist()) != set(files):
            raise ValueError(f'Archive verification failed: {path}')
        for name, file in files.items():
            if hashlib.sha256(zipped.read(name)).hexdigest() != digest(file):
                raise ValueError(f'Archive file differs: {name}')
    return {'sha256': digest(path), 'bytes': path.stat().st_size, 'file_count': len(files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    parser.add_argument('--validation', type=Path, required=True, help='windows-tests.json from test-windows.py')
    parser.add_argument('--tag', help='Version tag for a release; otherwise create a development package')
    args = parser.parse_args()
    build, output = args.build_dir.resolve(), args.output_dir.resolve()
    state, release = source_state(), version()
    label = package_version(release, state['commit'], args.tag)
    if state['dirty']:
        raise ValueError('Commit source changes before packaging so the build has an exact revision')
    verify_dependencies()
    report = json.loads(args.validation.read_text(encoding='utf-8'))
    validate_tests(report, build / 'Quickslot.dll', state, release)
    if not (build / 'Quickslot.pdb').is_file():
        raise ValueError('Missing matching build symbols: Quickslot.pdb')
    if output.exists() and any(output.iterdir()):
        raise ValueError(f'Output directory must be empty: {output}')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='quickslot-package-') as temporary:
        stage = Path(temporary)
        runtime = stage / 'runtime'
        subprocess.run(['cmake', '--install', str(build), '--config', 'Release', '--prefix', str(runtime)], check=True)
        files = {p.relative_to(runtime).as_posix(): p for p in runtime.rglob('*') if p.is_file()}
        validate_runtime(files)
        if digest(runtime / 'SKSE/Plugins/Quickslot.dll') != report['dll_sha256']:
            raise ValueError('Installed DLL differs from the tested binary')
        for file in (runtime / 'Quickslot').rglob('*.json'):
            json.loads(file.read_text(encoding='utf-8'))
        info = stage / 'SOURCE-INFO.json'
        info.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')
        sources = {f'Quickslot-{label}-source/{n}': p for n, p in source_files().items()}
        sources[f'Quickslot-{label}-source/SOURCE-INFO.json'] = info
        sets = {
            f'Quickslot-{label}.zip': files,
            f'Quickslot-{label}-source.zip': sources,
            f'Quickslot-{label}-symbols.zip': {'Quickslot.pdb': build / 'Quickslot.pdb'},
            f'Quickslot-{label}-api.zip': {
                **{p.name: p for p in (ROOT / 'src/API').iterdir() if p.is_file()},
                'COPYING.txt': ROOT / 'licenses/COPYING.txt',
                'EXCEPTIONS.md': ROOT / 'licenses/EXCEPTIONS.md',
            },
        }
        records = {}
        for name, entries in sets.items():
            records[name] = archive(output / name, entries, state['timestamp'])
            print(f'Verified {name}: {len(entries)} files')
    manifest = {
        'version': release, 'package_version': label, 'source': state,
        'workflow_run': os.environ.get('GITHUB_RUN_ID'), 'dll_sha256': report['dll_sha256'],
        'windows_tests': report, 'archives': records,
        'test_scope': 'Automated DLL, model and interface probes. Gameplay and physical controller/headset checks require Skyrim. Tagged releases also require the Linux tests to pass.',
    }
    (output / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    notes = release_notes(release)
    notes += '\nAssets: main mod package, complete source, debug symbols, menu API and SHA-256 checksums.\n'
    notes += '\nGame and hardware testing: [gameplay checks](https://github.com/opteron-x86/quickslot-hotkeys/blob/' + state['commit'] + '/docs/TESTING.md).\n'
    (output / 'release-notes.md').write_text(notes, encoding='utf-8')
    sums = ''.join(f'{digest(p)}  {p.name}\n' for p in sorted(output.iterdir()) if p.is_file())
    (output / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')
    print(f'Packages: {output}')


if __name__ == '__main__':
    main()

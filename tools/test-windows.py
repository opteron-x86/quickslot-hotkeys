#!/usr/bin/env python3
"""Run Windows DLL, model and integration probes."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from release_support import digest, source_state, version

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-release')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dll', type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    windows = os.name == 'nt'
    report = {'version': version(), 'source': source_state(),
              'dll_sha256': digest(args.dll or build / 'Quickslot.dll'),
              'passed': False, 'cases': {}}
    report_file = args.output / 'windows-tests.json'
    report_file.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def path(p):
        absolute = str(p.resolve())
        return absolute if windows else 'Z:' + absolute.replace('/', '\\')

    cases = {
        'plugin': [path(args.dll or build / 'Quickslot.dll')],
        'runtime_vr': [],
        'smart_equipment': [],
        'shortcut_library': [],
        'interactions': [],
        'localization': [path(ROOT)],
        'dx_state': [],
        'pngii': [path(build / 'pngii-legacy/PNGIconInjector.dll'),
                  path(build / 'pngii-incompatible/PNGIconInjector.dll'),
                  path(build / 'pngii-mock/PNGIconInjector.dll')],
        'controller_voice': [],
        'icon': [],
        'live': [path(ROOT)],
        'features': [],
        'hand_actions': [],
        'advanced': [path(ROOT)],
        'consumable': [path(ROOT)],
        'smf': [path(build / 'smf-incomplete/SKSEMenuFramework.dll'),
                path(build / 'smf-mock/SKSEMenuFramework.dll')],
    }
    for name, arguments in cases.items():
        executable = build / f'quickslot_{name}_probe.exe'
        command = ([str(executable)] if windows else ['wine', str(executable)]) + arguments
        with (args.output / f'windows-{name}.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        report['cases'][name] = {'returncode': result.returncode}
        report_file.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            raise SystemExit(f'FAIL: {name}, exit {result.returncode}; see {log.name}')
        print(f'PASS: {name}', flush=True)
    report['passed'] = True
    report_file.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()

"""Shared version, source and dependency checks for local and CI packaging."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATHS = (
    '.github', 'src', 'tests', 'third_party', 'licenses', 'assets', 'cmake',
    'vendor', 'tools', 'docs', '.gitignore', '.gitattributes',
    'CMakeLists.txt', 'CMakePresets.json', 'Quickslot.ini', 'vcpkg.json',
    'README.md', 'BUILDING.md', 'THEMES.md', 'CHANGELOG.md',
)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def version(root=ROOT):
    match = re.search(r'version\[\]\s*=\s*"([^"]+)"', (root / 'src/Version.h').read_text(encoding='utf-8'))
    if not match or not re.fullmatch(r'\d+\.\d+\.\d+(?:-(?:(?:alpha|beta|rc)\.\d+|rc\d+))?', match[1]):
        raise ValueError('Invalid release version in src/Version.h')
    return match[1]


def package_version(release, revision, tag=None):
    if tag:
        if tag != 'v' + release:
            raise ValueError(f'Tag {tag} does not match src/Version.h (v{release})')
        return release
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Development packages require a full Git commit ID')
    return f'{release}-dev.{revision[:12]}'


def source_state(root=ROOT):
    if (root / '.git').exists():
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
        status = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=all'], cwd=root, text=True)
        epoch = int(subprocess.check_output(['git', 'show', '-s', '--format=%ct', 'HEAD'], cwd=root, text=True))
        return {'commit': revision, 'dirty': bool(status), 'timestamp': epoch}
    info = root / 'SOURCE-INFO.json'
    if info.exists():
        return json.loads(info.read_text(encoding='utf-8'))
    raise ValueError('Use a Git checkout or the complete release source archive')


def source_files(root=ROOT):
    files = {}
    for name in SOURCE_PATHS:
        path = root / name
        if not path.exists():
            raise ValueError(f'Missing source path: {name}')
        for file in path.rglob('*') if path.is_dir() else [path]:
            if any(p in {'.git', '__pycache__'} for p in file.parts) or file.suffix == '.pyc':
                continue
            if file.is_symlink():
                raise ValueError(f'Source packages may not contain symlinks: {file}')
            if file.is_file():
                files[file.relative_to(root).as_posix()] = file
    return files


def verify_dependencies(root=ROOT):
    vendor = root / 'vendor'
    metadata = json.loads((vendor / 'dependencies.json').read_text(encoding='utf-8'))
    actual = {p.relative_to(vendor).as_posix() for p in vendor.rglob('*')
              if p.is_file() and '__pycache__' not in p.parts and p.name != 'dependencies.json'}
    if actual != set(metadata['files']):
        raise ValueError('Vendored file list differs from vendor/dependencies.json')
    for name, expected in metadata['files'].items():
        if digest(vendor / name) != expected:
            raise ValueError(f'Vendored dependency changed: {name}')
    return len(actual)


def release_notes(release, root=ROOT):
    text = (root / 'CHANGELOG.md').read_text(encoding='utf-8')
    marker = f'## {release}\n'
    if marker not in text:
        raise ValueError(f'Missing changelog entry for {release}')
    return text.split(marker, 1)[1].split('\n## ', 1)[0].strip() + '\n'

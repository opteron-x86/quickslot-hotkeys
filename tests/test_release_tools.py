"""Check release boundaries that could package the wrong build or user files."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import release_support as support
spec = importlib.util.spec_from_file_location('package_release', support.ROOT / 'tools/package-release.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class ReleaseTools(unittest.TestCase):
    def test_tag_must_match_version(self):
        self.assertEqual(support.package_version('0.9.0-rc20', 'a' * 40, 'v0.9.0-rc20'), '0.9.0-rc20')
        with self.assertRaises(ValueError):
            support.package_version('0.9.0-rc20', 'a' * 40, 'v0.9.0')

    def test_development_filename_identifies_commit(self):
        self.assertEqual(support.package_version('0.9.0-rc20', 'b' * 40), '0.9.0-rc20-dev.bbbbbbbbbbbb')
        with self.assertRaises(ValueError):
            support.package_version('0.9.0', 'not-a-commit')

    def test_release_notes_are_current_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'CHANGELOG.md').write_text('# Changelog\n\n## 1.1.0\n\nNew feature.\n\n## 1.0.0\n\nFirst release.\n', encoding='utf-8')
            self.assertEqual(support.release_notes('1.1.0', root), 'New feature.\n')
            with self.assertRaises(ValueError):
                support.release_notes('1.2.0', root)

    def test_source_archive_has_build_tools_and_art_sources(self):
        files = support.source_files()
        for name in ['README.md', 'tools/package-release.py', '.github/workflows/build.yml',
                     'assets/icons/source/sword.png', 'vendor/commonlib/CMakeLists.txt']:
            self.assertIn(name, files)
        self.assertFalse(any(n.startswith(('build/', 'dist/', '.git/')) for n in files))

    def test_vendor_hashes_after_pruning(self):
        self.assertGreater(support.verify_dependencies(), 1000)

    def test_runtime_rejects_user_data_and_build_outputs(self):
        base = dict.fromkeys(['SKSE/Plugins/Quickslot.dll', 'SKSE/Plugins/Quickslot.ini',
                             'Quickslot/icons/items.png', 'Quickslot/icons/manifest.json', 'Quickslot/README.md'])
        package.validate_runtime(base)
        for bad in ['Quickslot/settings.json', 'Quickslot/Presets/test.json', 'Quickslot/save.ess',
                    'Quickslot/debug.pdb', 'Quickslot/source/file.cpp', 'Quickslot/../other.txt',
                    'SKSE/Plugins/Other.dll', 'Quickslot/Other.dll', 'controlmap.txt']:
            with self.subTest(path=bad), self.assertRaises(ValueError):
                package.validate_runtime({**base, bad: None})
        del base['SKSE/Plugins/Quickslot.dll']
        with self.assertRaises(ValueError):
            package.validate_runtime(base)

    def test_archive_is_verified_and_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            file = root / 'sample.txt'; file.write_text('sample', encoding='utf-8')
            archive = root / 'test.zip'
            record = package.archive(archive, {'sample.txt': file}, 0)
            self.assertEqual(record['sha256'], support.digest(archive))
            with zipfile.ZipFile(archive) as zipped:
                self.assertEqual(zipped.read('sample.txt'), b'sample')
            with self.assertRaises(ValueError):
                package.archive(archive, {'sample.txt': file}, 0)

    def test_report_requires_every_probe_and_same_binary_revision(self):
        names = ['plugin', 'runtime_vr', 'smart_equipment', 'shortcut_library', 'interactions',
                 'localization', 'dx_state', 'controller_voice', 'icon', 'live', 'features',
                 'hand_actions', 'advanced', 'consumable', 'smf']
        with tempfile.TemporaryDirectory() as tmp:
            dll = Path(tmp) / 'Quickslot.dll'; dll.write_bytes(b'tested-binary')
            state = {'commit': 'a' * 40, 'dirty': False, 'timestamp': 100}
            report = {'passed': True, 'source': state, 'version': '0.9.0', 'dll_sha256': support.digest(dll),
                      'cases': {n: {'returncode': 0} for n in names}}
            package.validate_tests(report, dll, state, '0.9.0')
            for change in [{'passed': False}, {'version': '0.8.0'}, {'dll_sha256': 'wrong'},
                           {'source': {**state, 'commit': 'b' * 40}}, {'cases': {}}]:
                with self.subTest(change=change), self.assertRaises(ValueError):
                    package.validate_tests({**report, **change}, dll, state, '0.9.0')
            report['cases']['plugin']['returncode'] = 1
            with self.assertRaises(ValueError):
                package.validate_tests(report, dll, state, '0.9.0')


if __name__ == '__main__':
    unittest.main()

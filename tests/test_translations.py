import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


class TranslationTests(unittest.TestCase):
    def test_english_strings_match_the_source(self):
        result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/check-translations.py')], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, 'Run tools/check-translations.py --fix:\n' + result.stdout)


if __name__ == '__main__':
    unittest.main()

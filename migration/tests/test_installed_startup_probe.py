#!/usr/bin/env python3
"""Controls for the installed-startup gate; not application feature coverage."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('startup', ROOT/'tools/check_installed_startup.py')
startup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(startup)


class StartupReceiptTests(unittest.TestCase):
    def observation(self):
        return {'run_id': 'current', 'windows': [{'title': 'startup-current.xcf – GIMP',
                'visible': True, 'width': 800, 'height': 600}]}

    def test_visible_current_image(self):
        self.assertTrue(startup.gui_observation_valid(self.observation(), 'current', 'startup-current.xcf'))

    def test_other_run_cannot_acknowledge(self):
        self.assertFalse(startup.gui_observation_valid(self.observation(), 'previous', 'startup-current.xcf'))

    def test_other_image_cannot_acknowledge(self):
        self.assertFalse(startup.gui_observation_valid(self.observation(), 'current', 'other.xcf'))

    def test_hidden_or_empty_window_cannot_acknowledge(self):
        for field, value in [('visible', False), ('width', 0), ('height', 0)]:
            with self.subTest(field=field):
                row = self.observation()
                row['windows'][0][field] = value
                self.assertFalse(startup.gui_observation_valid(row, 'current', 'startup-current.xcf'))

    def test_module_set_and_bytes_remain_bound_to_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            directory = bundle/'usr/lib/x86_64-linux-gnu/gimp/3.0/modules'
            directory.mkdir(parents=True)
            module = directory/'module.so'
            module.write_bytes(b'staged module fixture')
            manifest = {str(module.relative_to(bundle)): {'sha256': startup.sha(module)}}
            (bundle/'file-manifest.json').write_text(json.dumps(manifest))
            self.assertEqual(startup.installed_modules(bundle), [str(module)])
            module.unlink()
            with self.assertRaisesRegex(RuntimeError, 'module set'):
                startup.installed_modules(bundle)
            module.write_bytes(b'replacement module')
            with self.assertRaisesRegex(RuntimeError, 'module differs'):
                startup.installed_modules(bundle)
            module.write_bytes(b'staged module fixture')
            (directory/'extra.so').write_bytes(b'unrecorded module')
            with self.assertRaisesRegex(RuntimeError, 'module set'):
                startup.installed_modules(bundle)


if __name__ == '__main__':
    unittest.main()

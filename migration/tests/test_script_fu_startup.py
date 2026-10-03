#!/usr/bin/env python3
"""Real Script-Fu startup, init selection and directory constants regressions.

Invoked by Meson with gimp_run_env. Every case launches a separate isolated
profile through in-build-gimp.py, checks the batch result, and rejects child
crash diagnostics even when GIMP itself exits zero.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / 'tools/in-build-gimp.py'
CRASH = re.compile(r'fatal error:|Segmentation fault|AddressSanitizer|'
                   r'UndefinedBehaviorSanitizer|runtime error:|'
                   r'assertion .* failed|batch command experienced', re.I)


class ScriptFuStartupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(
            prefix='script-fu-startup-', dir=os.environ['GIMP_GLOBAL_BUILD_ROOT'])
        cls.root = Path(cls.temporary.name)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.roots = []
        for index in range(2):
            root = cls.root / ('root-' + str(index))
            init = root / 'scriptfu-init'
            shutil.copytree(ROOT / 'plug-ins/script-fu/scripts/init', init,
                            ignore=shutil.ignore_patterns('meson.build'))
            with (init / 'init.scm').open('a') as stream:
                stream.write(f'\n(define startup-root-marker {index + 1})\n')
            cls.roots.append(root)

    def check_startup(self, paths=None, expected_marker=None, option='--gimprc',
                      expected_warning=None, no_data=False):
        expressions = [
            '(if (eq? (string->symbol script-fu-sys-init-directory) '
            '(string->symbol (string-append gimp-data-directory DIR-SEPARATOR "scripts" '
            'DIR-SEPARATOR "scriptfu-init"))) #t (error "wrong system root"))',
            '(if (eq? (string->symbol script-fu-user-init-directory) '
            '(string->symbol (string-append gimp-directory DIR-SEPARATOR "scripts" '
            'DIR-SEPARATOR "scriptfu-init"))) #t (error "wrong user root"))',
        ]
        if expected_marker is not None:
            expressions.append(f'(if (= startup-root-marker {expected_marker}) '
                               '#t (error "configured init order changed"))')
        if paths is None:
            expressions.append('(if (gimp-pdb-procedure-exists "script-fu-add-bevel") '
                               '#t (error "production extension scripts missing"))')
        expressions.append('(display "SCRIPT_FU_STARTUP_OK\\n")')
        arguments = ['-nidfs' if no_data else '-nifs',
                     '--batch-interpreter=plug-in-script-fu-eval']
        if paths is not None:
            config = self.root / (self._testMethodName + '.gimprc')
            config.write_text('(script-fu-path ' + json.dumps(os.pathsep.join(map(str, paths))) + ')\n')
            if option.endswith('='):
                arguments.append(option + str(config))
            else:
                arguments.extend([option, str(config)])
        arguments.extend(['-b', '(begin ' + ' '.join(expressions) + ')', '--quit'])
        result = subprocess.run([sys.executable, str(WRAPPER), *arguments],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=60)
        output = result.stdout
        # RUNNING logs include the source string; require a real output line.
        self.assertEqual(result.returncode, 0, output)
        self.assertIsNone(CRASH.search(output), output)
        self.assertRegex(output, r'(?m)^SCRIPT_FU_STARTUP_OK\s*$', output)
        if expected_warning:
            self.assertIn(expected_warning, output)
        print(f'{self._testMethodName}: clean startup and batch evaluation', flush=True)

    def test_default_production_scripts_and_no_data_quit(self):
        self.check_startup(no_data=True)

    def test_empty_path(self):
        self.check_startup([], expected_warning='Missing paths.')

    def test_single_path(self):
        self.check_startup([self.roots[0]], 1, option='-g')

    def test_two_paths(self):
        self.check_startup(self.roots, 1)

    def test_reversed_paths(self):
        self.check_startup(list(reversed(self.roots)), 2, option='--gimprc=')

    def test_missing_first_path(self):
        self.check_startup([self.root / 'does-not-exist', self.roots[1]], 2)

    def test_missing_all_paths(self):
        self.check_startup([self.root / 'does-not-exist'],
                           expected_warning='Failed to load main initialization file')


if __name__ == '__main__':
    unittest.main()

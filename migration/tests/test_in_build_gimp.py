#!/usr/bin/env python3
"""Exercise the actual in-build wrapper with a harmless fake GIMP child."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / 'tools/in-build-gimp.py'
spec = importlib.util.spec_from_file_location('in_build_gimp', WRAPPER)
wrapper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wrapper)


class WrapperTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='gimp-wrapper-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        self.build = self.root / 'build'
        self.build.mkdir()
        self.scripts = self.source / 'plug-ins/script-fu/scripts'
        self.scripts.mkdir(parents=True)
        (self.build / 'meson-info').mkdir()
        self.install_map = {}
        for name, destination in (
            ('ordinary.scm', 'scripts/ordinary.scm'),
            ('init/init.scm', 'scripts/scriptfu-init/init.scm'),
            ('init/plug-in-compat.scm', 'scripts/scriptfu-init/plug-in-compat.scm'),
            ('test-sphere-v3.scm', 'plug-ins/test-sphere-v3/test-sphere-v3.scm'),
            ('test/development.scm', 'scripts/development.scm'),
        ):
            source = self.scripts / name
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text('; current source ' + name)
            self.install_map[str(source)] = '/installed/share/gimp/3.0/' + destination
        (self.scripts / 'not-installed.scm').write_text('; must not be staged')
        self.write_map()
        self.existing = self.root / 'existing-profile'
        self.existing.mkdir()
        (self.existing / 'gimprc').write_text('; existing profile, keep byte-exact\n')
        self.snapshot = self.root / 'child.json'
        self.child = self.root / 'fake-gimp'
        self.child.write_text('''#!/usr/bin/env python3
import json, os, pathlib, sys
profile = pathlib.Path(os.environ['GIMP3_DIRECTORY'])
json.dump({'argv': sys.argv[1:], 'profile': str(profile),
           'files': {p.relative_to(profile).as_posix(): p.read_text()
                     for p in profile.rglob('*') if p.is_file()}},
          open(os.environ['WRAPPER_SNAPSHOT'], 'w'))
sys.exit(int(os.environ.get('WRAPPER_EXIT', '0')))
''')
        self.child.chmod(0o755)
        self.environment = dict(os.environ,
                                GIMP_GLOBAL_BUILD_ROOT=str(self.build),
                                GIMP_GLOBAL_SOURCE_ROOT=str(self.source),
                                GIMP_SELF_IN_BUILD=str(self.child),
                                GIMP_PYTHON_WITH_GI=sys.executable,
                                GIMP3_DIRECTORY=str(self.existing),
                                WRAPPER_SNAPSHOT=str(self.snapshot))
        self.environment.pop('GIMP_DEBUG_SELF', None)
        self.environment.pop('GIMP_TEMP_UPDATE_RPATH', None)

    def write_map(self):
        (self.build / 'meson-info/intro-installed.json').write_text(
            json.dumps(self.install_map))

    def invoke(self, arguments, exit_code=0):
        self.environment['WRAPPER_EXIT'] = str(exit_code)
        result = subprocess.run([sys.executable, str(WRAPPER), *arguments],
                                env=self.environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, exit_code, result.stdout + result.stderr)
        snapshot = json.loads(self.snapshot.read_text())
        self.assertEqual(snapshot['argv'], arguments)
        self.assertFalse(Path(snapshot['profile']).exists())
        self.assertEqual(list(self.build.glob('.GIMP3-build-config-*')), [])
        self.assertEqual((self.existing / 'gimprc').read_text(),
                         '; existing profile, keep byte-exact\n')
        return snapshot

    def test_stages_install_map_only_and_keeps_current_init(self):
        snapshot = self.invoke(['--batch', '(display "hello")', '--quit'])
        self.assertEqual(set(snapshot['files']), {
            'gimprc', 'scripts/ordinary.scm', 'scripts/scriptfu-init/init.scm',
            'scripts/scriptfu-init/plug-in-compat.scm',
        })
        self.assertEqual(snapshot['files']['scripts/scriptfu-init/init.scm'],
                         '; current source init/init.scm')
        self.assertEqual(snapshot['files']['gimprc'],
                         '(script-fu-path "${gimp_dir}/scripts")\n')

    def test_explicit_config_forms_and_batch_arguments_survive(self):
        config = self.root / 'explicit config'
        config.write_text('(script-fu-path "")\n')
        for option in (['--gimprc', str(config)], ['--gimprc=' + str(config)],
                       ['-g', str(config)], ['-g' + str(config)],
                       ['-nisg', str(config)],
                       ['--system-gimprc', str(config)],
                       ['--system-gimprc=' + str(config)]):
            with self.subTest(option=option):
                snapshot = self.invoke([*option, '-b', '--gimprc', '--quit'])
                self.assertNotIn('gimprc', snapshot['files'])
                self.assertEqual(config.read_text(), '(script-fu-path "")\n')

    def test_batch_text_is_never_parsed_as_a_config_option(self):
        for arguments in (['-b', '--gimprc'], ['-nisb', '--system-gimprc'],
                          ['--batch', '--gimprc'], ['--batch=--gimprc'],
                          ['-b--gimprc'], ['--', '--gimprc']):
            with self.subTest(arguments=arguments):
                snapshot = self.invoke(arguments)
                self.assertIn('gimprc', snapshot['files'])

    def test_cleanup_and_exit_status_on_child_failure(self):
        self.invoke(['-b', '(error "intentional")'], exit_code=23)

    def test_missing_init_fails_before_child_and_cleans_profile(self):
        del self.install_map[str(self.scripts / 'init/init.scm')]
        self.write_map()
        result = subprocess.run([sys.executable, str(WRAPPER)],
                                env=self.environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('contains no Script-Fu init.scm', result.stderr)
        self.assertFalse(self.snapshot.exists())
        self.assertEqual(list(self.build.glob('.GIMP3-build-config-*')), [])


if __name__ == '__main__':
    unittest.main()

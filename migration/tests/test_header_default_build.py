#!/usr/bin/env python3
"""Exercise the real header-probe Meson graph with tiny isolated C/C++ inputs.

This is a build-graph regression, not evidence for compiling GIMP's real headers.
The positive projects copy the production meson.build byte-for-byte. Only the
negative-control copies change default-target membership. Every project starts
empty, runs plain Ninja, then runs its tests without rebuilding. No GIMP build,
production generator, source inventory, or existing build directory is changed.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
HEADER_DIR = Path('app/painter/tests/headers')
RECIPE = ROOT / HEADER_DIR / 'meson.build'
SCOPES = ('c-header-linkage.json', 'cpp-header-boundary.json',
          'generated-c-header-scope.json', 'additional-c-header-routes.json')
TARGETS = ('painter-public-headers-c', 'painter-public-headers-cpp')

# The generator deliberately has no GIMP dependencies. Its interface and output
# kinds match the production generator so that the *real recipe* owns the graph.
GENERATOR = r'''import argparse
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser()
for option in ('scope', 'private-scope', 'generated-scope', 'additional-scope'):
    parser.add_argument('--' + option, type=Path)
parser.add_argument('--http', choices=('0', '1'), required=True)
parser.add_argument('--list', action='store_true')
parser.add_argument('--list-inputs', action='store_true')
parser.add_argument('--output-dir', type=Path)
parser.add_argument('--verify-build', type=Path)
parser.add_argument('--library', type=Path, action='append', default=[])
parser.add_argument('--report', type=Path)
args = parser.parse_args()
sources = {
    'probe-c.c': '#ifdef __cplusplus\n#error C compiler required\n#endif\n'
                 '#include "fixture-input.h"\n'
                 'int fixture_c(void) { return FIXTURE_VALUE; }\n',
    'probe-cpp.cpp': '#ifndef __cplusplus\n#error C++ compiler required\n#endif\n'
                     '#include "fixture-input.h"\n'
                     'int fixture_cpp() { return FIXTURE_VALUE; }\n',
}
if args.http == '1':
    sources['probe-http.cpp'] = ('#ifndef __cplusplus\n#error C++ compiler required\n'
                                 '#endif\nint fixture_http() { return 1; }\n')
manifest = {'fixture': 'header-default-build', 'http': args.http,
            'input_sha256': hashlib.sha256((root / 'fixture-input.h').read_bytes()).hexdigest(),
            'sources': sources}
if args.list:
    print('\n'.join([*sources, 'header-probes.json']))
elif args.list_inputs:
    print('fixture-input.h')
elif args.output_dir:
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, body in sources.items():
        (args.output_dir / name).write_text(body)
    (args.output_dir / 'header-probes.json').write_text(json.dumps(manifest))
elif args.verify_build:
    path = args.verify_build / 'header-probes.json'
    if not path.is_file():
        raise SystemExit('FIXTURE_MISSING_MANIFEST: ' + str(path))
    if json.loads(path.read_text()) != manifest:
        raise SystemExit('FIXTURE_CHANGED_MANIFEST')
    for name, body in sources.items():
        if (args.verify_build / name).read_text() != body:
            raise SystemExit('FIXTURE_CHANGED_SOURCE: ' + name)
    if len(args.library) != 2:
        raise SystemExit('FIXTURE_EXPECTED_TWO_LIBRARIES')
    for library in args.library:
        if not library.is_file() or library.stat().st_size == 0:
            raise SystemExit('FIXTURE_MISSING_LIBRARY: ' + str(library))
    args.report.write_text(json.dumps({'status': 'PASS', 'fixture': True}))
    print('PASS: isolated default-build C/C++ graph')
else:
    parser.error('expected list, generation, or verification operation')
'''


def change_default(recipe, variable, enabled):
    """Change one target in a negative copy, never the production recipe."""
    start = re.search(r'^' + re.escape(variable) + r'\s*=', recipe, re.M)
    if start is None:
        raise AssertionError('Cannot find production target variable: ' + variable)
    following = re.search(r'^(?:[A-Za-z_]\w*\s*=|test\s*\()',
                          recipe[start.end():], re.M)
    end = start.end() + following.start() if following else len(recipe)
    block, count = re.subn(r'\bbuild_by_default\s*:\s*(?:true|false)\b',
                          'build_by_default: ' + ('true' if enabled else 'false'),
                          recipe[start.start():end])
    if count != 1:
        raise AssertionError('Expected one explicit default-build flag for ' + variable)
    return recipe[:start.start()] + block + recipe[end:]


def fingerprint(paths):
    """Include absence and mtimes so a test-time rebuild cannot hide."""
    return {str(path): (hashlib.sha256(path.read_bytes()).hexdigest(),
                       path.stat().st_size, path.stat().st_mtime_ns)
            if path.is_file() else None for path in paths}


class HeaderDefaultBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.commands = {}
        for name in ('meson', 'ninja'):
            command = shutil.which(name)
            if not command:
                raise RuntimeError(
                    'Required executable %s is missing. Activate the project build/dependency '
                    'environment and install Meson, Ninja, and C/C++ compilers; this '
                    'regression must not be skipped.' % name)
            cls.commands[name] = command
        cls.original = RECIPE.read_bytes()

    def command(self, command, environment, expected_success=True):
        try:
            result = subprocess.run(command, env=environment, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=120)
        except (OSError, subprocess.TimeoutExpired) as error:
            self.fail('Cannot run fixture command %r: %s. Check the activated build '
                      'environment and its Meson/Ninja/C/C++ toolchain.' % (command, error))
        if expected_success:
            self.assertEqual(result.returncode, 0,
                             'Fixture command failed: %r\n%s\nCheck the activated '
                             'Meson/Ninja/C/C++ toolchain.' % (command, result.stdout))
        else:
            self.assertNotEqual(result.returncode, 0,
                                'Negative graph unexpectedly passed: %r\n%s' %
                                (command, result.stdout))
        return result

    def exercise(self, http=False, exclude=(), generator_only=False):
        with tempfile.TemporaryDirectory(prefix='header-default-build-') as temporary:
            source = Path(temporary) / 'source'
            build = Path(temporary) / 'build'
            headers = source / HEADER_DIR
            headers.mkdir(parents=True)
            inventory = source / 'migration/inventory'
            inventory.mkdir(parents=True)
            for name in SCOPES:
                (inventory / name).write_text('{}\n')
            (source / 'fixture-input.h').write_text('#define FIXTURE_VALUE 7\n')
            (headers / 'generate-probes.py').write_text(GENERATOR)
            # The second registered test is scaffolding only, not route coverage.
            (headers / 'test-additional-routes.py').write_text(
                'from pathlib import Path\n'
                'assert (Path(__file__).resolve().parents[4] / "fixture-input.h").is_file()\n')
            recipe = self.original.decode()
            for variable in exclude:
                recipe = change_default(recipe, variable, False)
            if generator_only:
                recipe = change_default(recipe, 'painter_header_sources', True)
            (headers / 'meson.build').write_bytes(recipe.encode())
            if not exclude and not generator_only:
                self.assertEqual((headers / 'meson.build').read_bytes(), self.original)
            (source / 'meson.build').write_text(
                "project('header-default-build-fixture', 'c', 'cpp')\n"
                "python = import('python').find_installation()\n"
                "cc = meson.get_compiler('c')\n"
                "cxx = meson.get_compiler('cpp')\n"
                "rootInclude = include_directories('.')\n"
                "rootAppInclude = include_directories('app')\n"
                "gegl = declare_dependency()\n"
                "gtk3 = declare_dependency()\n"
                "gexiv2 = declare_dependency()\n"
                "json_glib = declare_dependency()\n"
                "painter_brush_settings_dep = declare_dependency()\n"
                "painter_cpp_args = []\n"
                "painter_http_enabled = %s\n" % ('true' if http else 'false') +
                "subdir('app/painter/tests/headers')\n")
            environment = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
            self.command([self.commands['meson'], 'setup', '--backend=ninja',
                          str(build), str(source)], environment)
            targets = json.loads((build / 'meson-info/intro-targets.json').read_text())
            archives = []
            for name in TARGETS:
                matching = [target for target in targets if target['name'] == name]
                self.assertEqual(len(matching), 1, name)
                self.assertEqual(len(matching[0]['filename']), 1, name)
                archives.append(Path(matching[0]['filename'][0]))
            generated = build / HEADER_DIR
            outputs = [generated / 'header-probes.json', generated / 'probe-c.c',
                       generated / 'probe-cpp.cpp', *archives]
            if http:
                outputs.append(generated / 'probe-http.cpp')
            self.assertTrue(all(not path.exists() for path in outputs),
                            'Setup must not prepopulate the default-build artifacts')
            self.command([self.commands['ninja'], '-C', str(build)], environment)
            self.assertTrue((generated / 'header-probes.json').is_file(),
                            'Plain Ninja did not build the header manifest; the test '
                            'archives must be reachable from the default target')
            for variable, archive in zip(('painter_header_c', 'painter_header_cpp'), archives):
                self.assertEqual(archive.is_file(), variable not in exclude,
                                 'Unexpected default-build membership: ' + str(archive))
                if archive.is_file():
                    self.assertGreater(archive.stat().st_size, 0)
            before = fingerprint(outputs)
            report = generated / 'header-compile-result.json'
            self.assertFalse(report.exists())
            result = self.command([self.commands['meson'], 'test', '-C', str(build),
                                   '--no-rebuild', '--num-processes', '1', '--print-errorlogs'],
                                  environment, expected_success=not exclude)
            self.assertEqual(fingerprint(outputs), before,
                             'No-rebuild verification changed a generated source or archive')
            results = [json.loads(line) for line in
                       (build / 'meson-logs/testlog.json').read_text().splitlines() if line]
            self.assertEqual(len(results), 2, 'Expected both unfiltered fixture tests')
            primary = [row for row in results
                       if row['name'].split(' / ')[-1] == 'painter-public-header-compilation']
            self.assertEqual(len(primary), 1, results)
            self.assertEqual(primary[0]['result'], 'FAIL' if exclude else 'OK')
            self.assertTrue(all(row['result'] == 'OK' for row in results
                                if row is not primary[0]), results)
            if exclude:
                self.assertIn('FIXTURE_MISSING_LIBRARY:', result.stdout)
                self.assertFalse(report.exists())
            else:
                self.assertEqual(json.loads(report.read_text()), {'status': 'PASS', 'fixture': True})
            self.assertEqual(RECIPE.read_bytes(), self.original,
                             'Production recipe changed during the isolated regression')

    def test_plain_default_build_then_no_rebuild(self):
        for http in (False, True):
            with self.subTest(http=http):
                self.exercise(http=http)

    def test_excluding_c_archive_is_detected(self):
        self.exercise(exclude=('painter_header_c',))

    def test_excluding_cpp_archive_is_detected(self):
        self.exercise(exclude=('painter_header_cpp',))

    def test_default_generator_alone_is_insufficient(self):
        self.exercise(exclude=('painter_header_c', 'painter_header_cpp'), generator_only=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Exercise the resource adapter with real GLib and an isolated Meson project.

Run in the native dependency environment, optionally with --output-dir to keep
the fixture, commands and outputs. No GIMP build directory is configured.
"""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / 'tools/gimp-compile-resources.py.in'
COMMANDS = []


def run(command, cwd, check=True):
    command = [str(arg) for arg in command]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    COMMANDS.append(dict(command=command, cwd=str(cwd), status=result.returncode,
                         stdout=result.stdout, stderr=result.stderr))
    if check and result.returncode:
        raise AssertionError(shlex.join(command) + '\n' + result.stdout + result.stderr)
    return result


def configure_wrapper(path, compiler):
    path.write_text(TEMPLATE.read_text().replace('@GLIB_COMPILE_RESOURCES@',
                                               json.dumps(str(compiler))))
    path.chmod(0o755)


class ResourceCompilerTests(unittest.TestCase):
    def setUp(self):
        self.work = OUTPUT / self._testMethodName
        self.work.mkdir()
        (self.work / 'payload.txt').write_text('Resource linkage fixture\n')
        (self.work / 'fixture.gresource.xml').write_text(
            '<gresources><gresource prefix="/fixture">'
            '<file>payload.txt</file></gresource></gresources>\n')
        self.wrapper = self.work / 'adapter.py'
        configure_wrapper(self.wrapper, COMPILER)
        self.wrapped = [sys.executable, self.wrapper]
        self.flags = ['fixture.gresource.xml', '--c-name', 'fixture']

    def test_c_and_cpp_linkage(self):
        cflags = shlex.split(run(['pkg-config', '--cflags', 'gio-2.0'], self.work).stdout)
        libs = shlex.split(run(['pkg-config', '--libs', 'gio-2.0'], self.work).stdout)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        cxx = shlex.split(os.environ.get('CXX', 'c++'))
        for manual in (False, True):
            extra = ['--manual-register'] if manual else []
            run([COMPILER] + self.flags + extra +
                ['--generate-source', '--target', 'fixture.c'], self.work)
            run(self.wrapped + self.flags + extra +
                ['--generate', '--target', 'fixture.h'], self.work)
            header = (self.work / 'fixture.h').read_text()
            self.assertEqual(header.count('G_BEGIN_DECLS'), 1)
            self.assertEqual(header.count('G_END_DECLS'), 1)
            registration = ('auto reg = &fixture_register_resource; '
                            'auto unreg = &fixture_unregister_resource; '
                            'reg(); ' if manual else '')
            (self.work / 'consumer.cpp').write_text(
                '#include "fixture.h"\n#include "fixture.h"\n'
                'int main() { auto get = &fixture_get_resource; ' +
                registration + 'auto resource = get(); ' +
                ('unreg(); ' if manual else '') +
                'return resource ? 0 : 1; }\n')
            (self.work / 'consumer.c').write_text(
                '#include "fixture.h"\n#include "fixture.h"\n'
                'GResource *(*get_resource)(void) = &fixture_get_resource;\n')
            run(cc + ['-std=c11', '-Wall', '-Werror'] + cflags +
                ['-c', 'consumer.c', '-o', 'consumer.o'], self.work)
            run(cc + ['-std=c11'] + cflags +
                ['-c', 'fixture.c', '-o', 'fixture.o'], self.work)
            run(cxx + ['-std=c++14', '-Wall', '-Werror'] + cflags +
                ['consumer.cpp', 'fixture.o', '-o', 'consumer'] + libs, self.work)
            run([self.work / 'consumer'], self.work)

    def test_explicit_header_and_internal_manual_mode(self):
        run(self.wrapped + self.flags + ['--generate-header', '--internal',
            '--manual-register', '--target=declarations.inc'], self.work)
        text = (self.work / 'declarations.inc').read_text()
        for declaration in ('fixture_get_resource', 'fixture_register_resource',
                            'fixture_unregister_resource'):
            self.assertLess(text.index('G_BEGIN_DECLS'), text.index(declaration))
            self.assertLess(text.index(declaration), text.index('G_END_DECLS'))
        self.assertIn('G_GNUC_INTERNAL', text)

    def test_non_header_modes_are_identical(self):
        cases = [(['--generate-source'], 'resource.c'),
                 (['--generate-source'], 'source-named-header.h'),
                 (['--generate'], 'resource.c'),
                 ([], 'resource.gresource'),
                 (['--generate'], 'resource.gresource'),
                 (['--generate-source', '--dependency-file=resource.d',
                   '--generate-phony-targets'], 'resource.c')]
        for options, target in cases:
            arguments = self.flags + options + ['--target', target]
            direct = run([COMPILER] + arguments, self.work)
            expected = (self.work / target).read_bytes()
            depfile = self.work / 'resource.d'
            dep = depfile.read_bytes() if depfile.exists() else None
            wrapped = run(self.wrapped + arguments, self.work)
            self.assertEqual((wrapped.returncode, wrapped.stdout, wrapped.stderr),
                             (direct.returncode, direct.stdout, direct.stderr))
            self.assertEqual((self.work / target).read_bytes(), expected)
            if dep is not None:
                self.assertEqual(depfile.read_bytes(), dep)
        for arguments in (['--version'], ['--help'],
                          self.flags + ['--generate-dependencies']):
            direct = run([COMPILER] + arguments, self.work)
            wrapped = run(self.wrapped + arguments, self.work)
            self.assertEqual((wrapped.returncode, wrapped.stdout, wrapped.stderr),
                             (direct.returncode, direct.stdout, direct.stderr))

    def test_failed_compiler_preserves_status_diagnostics_and_stale_header(self):
        target = self.work / 'stale.h'
        target.write_text('Do not modify a stale output\n')
        arguments = ['missing.xml', '--generate', '--target=stale.h']
        direct = run([COMPILER] + arguments, self.work, check=False)
        wrapped = run(self.wrapped + arguments, self.work, check=False)
        self.assertNotEqual(direct.returncode, 0)
        self.assertEqual((wrapped.returncode, wrapped.stdout, wrapped.stderr),
                         (direct.returncode, direct.stdout, direct.stderr))
        self.assertEqual(target.read_text(), 'Do not modify a stale output\n')

    def test_version_does_not_rewrite_stale_header(self):
        run([COMPILER] + self.flags +
            ['--generate-header', '--target=stale.h'], self.work)
        original = (self.work / 'stale.h').read_bytes()
        run(self.wrapped + ['--version', '--generate', '--target=stale.h'], self.work)
        self.assertEqual((self.work / 'stale.h').read_bytes(), original)

    @unittest.skipUnless(os.name == 'posix', 'Unix signal status preservation')
    def test_partial_output_and_signal_failure(self):
        compiler = self.work / 'failing compiler.py'
        configure_wrapper(self.wrapper, compiler)
        for failure in ('sys.exit(37)', 'os.kill(os.getpid(), signal.SIGTERM)'):
            compiler.write_text(
                '#!/usr/bin/env python3\nimport os, signal, sys\n'
                'from pathlib import Path\n'
                'Path("partial.h").write_text("partial compiler output\\n")\n'
                'print("compiler diagnostic", file=sys.stderr, flush=True)\n' + failure + '\n')
            compiler.chmod(0o755)
            arguments = ['--generate', '--target=partial.h']
            direct = run([compiler] + arguments, self.work, check=False)
            wrapped = run(self.wrapped + arguments, self.work, check=False)
            self.assertEqual((wrapped.returncode, wrapped.stdout, wrapped.stderr),
                             (direct.returncode, direct.stdout, direct.stderr))
            self.assertIn(direct.returncode, (37, -signal.SIGTERM))
            self.assertEqual((self.work / 'partial.h').read_text(), 'partial compiler output\n')

    def test_existing_linkage_and_layout_validation(self):
        spec = importlib.util.spec_from_file_location('resource_adapter', self.wrapper)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        run([COMPILER] + self.flags +
            ['--generate-header', '--target=original.h'], self.work)
        original = (self.work / 'original.h').read_bytes()
        wrapped = module.add_c_linkage(original)
        self.assertEqual(module.add_c_linkage(wrapped), wrapped)
        explicit = wrapped.replace(b'G_BEGIN_DECLS', b'#ifdef __cplusplus\nextern "C" {\n#endif')
        explicit = explicit.replace(b'G_END_DECLS', b'#ifdef __cplusplus\n}\n#endif')
        self.assertEqual(module.add_c_linkage(explicit), explicit)
        crlf = module.add_c_linkage(original.replace(b'\n', b'\r\n'))
        self.assertNotIn(b'\n', crlf.replace(b'\r\n', b''))
        with self.assertRaises(ValueError):
            module.add_c_linkage(b'unrecognized header\n')
        with self.assertRaises(ValueError):
            module.add_c_linkage(wrapped.replace(b'G_END_DECLS', b''))

    def test_meson_native_and_cross_tool_selection(self):
        # Exercise the actual top-level integration instead of a second copy.
        root_meson = (ROOT / 'meson.build').read_text()
        block = re.search(r"resource_gio = dependency\('gio-2.0'.*?"
                          r'find_program\(resource_wrapper, native: true\)\)',
                          root_meson, re.DOTALL).group()
        source = self.work / 'source with spaces'
        source.mkdir()
        (source / 'tools').mkdir()
        shutil.copyfile(TEMPLATE, source / 'tools' / TEMPLATE.name)
        for name in ('fixture.gresource.xml', 'payload.txt'):
            shutil.copyfile(self.work / name, source / name)
        (source / 'consumer.cpp').write_text(
            '#include "ordinary.h"\n#include "ordinary.h"\n'
            '#include "manual.h"\n#include "manual.h"\n'
            'int main() { auto get = &ordinary_get_resource; '
            'auto reg = &manual_register_resource; '
            'auto unreg = &manual_unregister_resource; '
            'reg(); unreg(); return get() ? 0 : 1; }\n')
        (source / 'meson.build').write_text(
            "project('resource-adapter-fixture', 'c', 'cpp', meson_version: '>=0.61.0', "
            "default_options: ['cpp_std=c++14'])\n"
            "gnome = import('gnome')\n" + block + '\n' +
            "ordinary = gnome.compile_resources('ordinary', 'fixture.gresource.xml', "
            "c_name: 'ordinary')\n"
            "manual = gnome.compile_resources('manual', 'fixture.gresource.xml', "
            "c_name: 'manual', extra_args: ['--manual-register'])\n"
            "executable('consumer', 'consumer.cpp', ordinary, manual, "
            "dependencies: dependency('gio-2.0'))\n")
        cross_file = self.work / 'cross.ini'
        # This is a simulated cross configuration using runnable native C/C++
        # compilers. A poisoned target tool must never be selected or executed.
        cross_file.write_text(
            "[binaries]\nc = 'cc'\ncpp = 'c++'\npkg-config = 'pkg-config'\n"
            "glib-compile-resources = '/nonexistent/target/glib-compile-resources'\n"
            "[host_machine]\nsystem = 'linux'\ncpu_family = 'aarch64'\n"
            "cpu = 'aarch64'\nendian = 'little'\n")
        native_file = self.work / 'native.ini'
        native_file.write_text('[built-in options]\npkg_config_path = ' +
                               repr(os.environ.get('PKG_CONFIG_PATH_FOR_BUILD',
                                    os.environ.get('PKG_CONFIG_PATH', '')).split(os.pathsep)) + '\n')
        for name, options in [('native', []), ('cross', ['--cross-file', cross_file])]:
            build = self.work / name
            run(['meson', 'setup', build, source, '--native-file', native_file] + options,
                self.work)
            run(['meson', 'compile', '-C', build], self.work)
            run([build / 'consumer'], self.work)
            configured = (build / 'gimp-compile-resources.py').read_text()
            self.assertNotIn('/nonexistent/target', configured)
            for header in ('ordinary.h', 'manual.h'):
                text = (build / header).read_text()
                self.assertEqual(text.count('G_BEGIN_DECLS'), 1)
                self.assertEqual(text.count('G_END_DECLS'), 1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=shutil.which('glib-compile-resources'))
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if not args.compiler:
        parser.error('glib-compile-resources is required')
    COMPILER = Path(args.compiler).resolve()
    temporary = tempfile.TemporaryDirectory() if args.output_dir is None else None
    OUTPUT = args.output_dir.resolve() if args.output_dir else Path(temporary.name)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    try:
        result = unittest.TextTestRunner(verbosity=2).run(
            unittest.defaultTestLoader.loadTestsFromTestCase(ResourceCompilerTests))
        (OUTPUT / 'commands.json').write_text(json.dumps(COMMANDS, indent=2) + '\n')
        sys.exit(not result.wasSuccessful())
    finally:
        if temporary:
            temporary.cleanup()

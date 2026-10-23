#!/usr/bin/env python3
"""Compile and run the C/C++14 painter foundation without the whole GIMP build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'app/painter'
C_SOURCES = ['tests/test-c-api.c', 'tests/test-fixture.c', 'tests/test-hierarchy.c']
CPP_SOURCES = ['gimp-painter-error.cpp', 'gimp-painter-binding.cpp', 'binding-store.cpp']
TEST_CPP = ['tests/test-foundation.cpp', 'tests/test-resources.cpp', 'tests/test-gobject.cpp', 'tests/test-reentry.cpp', 'tests/test-hierarchy.cpp']
C_HEADERS = ['gimp-painter-error.h', 'gimp-painter-binding.h']
CPP_HEADERS = ['boundary.hpp', 'object-ref.hpp', 'binding-store.hpp',
               'resources.hpp', 'connection.hpp', 'source.hpp']


def run(build, sanitizer, leak_check=False):
    commands = []
    outputs = []

    def execute(argv, *, input=None, env=None, expected_error=None):
        result = subprocess.run(argv, input=input, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, env=env)
        commands.append([str(arg) for arg in argv])
        outputs.append(result.stdout)
        if expected_error:
            if result.returncode == 0 or expected_error not in result.stdout:
                raise RuntimeError('Expected compile-time borrow rejection: ' + result.stdout)
            return result.stdout
        if result.returncode:
            raise RuntimeError(f'{shlex.join(map(str, argv))}\n{result.stdout}')
        return result.stdout

    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    cflags = shlex.split(execute(['pkg-config', '--cflags', 'gobject-2.0']))
    libs = shlex.split(execute(['pkg-config', '--libs', 'gobject-2.0']))
    flags = ['-Wall', '-Wextra', '-Werror', '-g', '-fvisibility=hidden']
    if sanitizer:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    includes = ['-I' + str(MODULE), '-I' + str(MODULE / 'tests')]
    for language, compiler, standard, headers in [
        ('c', cc, '-std=c11', C_HEADERS),
        ('c++', cxx, '-std=c++14', C_HEADERS + CPP_HEADERS),
    ]:
        for header in headers:
            execute(compiler + flags + [standard] + cflags + includes +
                    ['-x', language, '-fsyntax-only', '-'],
                    input=f'#include "{header}"\n#include "{header}"\n')
    # The internal synchronous borrow must not become a returned Impl pointer
    # or reference. C object pointers required by real C APIs remain permitted.
    for method in ('initialize', 'with', 'read'):
        for result in ('pointer', 'reference'):
            const = 'const ' if method == 'read' else ''
            suffix = '*' if result == 'pointer' else '&'
            expression = '&impl' if result == 'pointer' else 'impl'
            probe = ('#include "binding-store.hpp"\n'
                     'using namespace GimpPainter;\n'
                     'struct Impl { void close() noexcept {} };\n'
                     'struct Slot : SlotSpec<GObject, Impl> {};\n'
                     'void probe(BindingStore& store) { store.' + method +
                     '<Slot>([](' + const + 'Impl& impl) -> ' + const + 'Impl' + suffix +
                     ' { return ' + expression + '; }); }\n')
            execute(cxx + flags + ['-std=c++14'] + cflags + includes +
                    ['-x', 'c++', '-fsyntax-only', '-'], input=probe,
                    expected_error='BindingStore borrow must not escape')
    objects = []
    library_objects = []
    for name in C_SOURCES + CPP_SOURCES + TEST_CPP:
        source = MODULE / name
        output = build / (source.name + '.o')
        is_c = source.suffix == '.c'
        execute((cc if is_c else cxx) + flags +
                (['-std=c11'] if is_c else ['-std=c++14', '-fno-rtti']) +
                cflags + includes + ['-c', str(source), '-o', str(output)])
        (library_objects if name in CPP_SOURCES else objects).append(str(output))
    archive = build / 'libapppainter.a'
    execute(['ar', 'rcs', str(archive)] + library_objects)
    executable = build / 'test-painter-foundation'
    # main.c is compiled by the C compiler; the static C++ library requires the
    # C++ runtime at final link, as the app's Meson target also does.
    execute(cxx + flags + objects + [str(archive)] + libs +
            ['-pthread', '-o', str(executable)])
    environment = os.environ.copy()
    if sanitizer:
        environment['ASAN_OPTIONS'] = f'detect_leaks={int(leak_check)}:halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    output = execute([str(executable)], env=environment)
    print(output, end='')
    tracked = [MODULE / name for name in C_SOURCES + CPP_SOURCES + TEST_CPP + C_HEADERS + CPP_HEADERS +
               ['tests/test-c-api.h', 'tests/test-fixture.h', 'tests/test-fixture-traits.hpp',
                'tests/test-hierarchy.h', 'tests/test-registry.hpp']]
    tracked.append(Path(__file__).resolve())
    return {'status': 'PASS', 'scope': 'standalone painter foundation, not full GIMP or legacy compatibility',
            'sanitizers': (['address', 'undefined'] + (['leak'] if leak_check else [])) if sanitizer else [],
            'leak_check': 'enabled' if leak_check else 'not run',
            'compiled_sources': ['app/painter/' + name for name in C_SOURCES + CPP_SOURCES + TEST_CPP],
            'commands': commands, 'output': outputs,
            'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in sorted(set(tracked))}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--leak-check', action='store_true', help='Enable LSan (requires sanitizer and ptrace support)')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.leak_check and not args.sanitize:
        parser.error('--leak-check requires --sanitize')
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
        report = run(args.build_dir.resolve(), args.sanitize, args.leak_check)
    else:
        with tempfile.TemporaryDirectory(prefix='painter-foundation-') as directory:
            report = run(Path(directory), args.sanitize, args.leak_check)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()

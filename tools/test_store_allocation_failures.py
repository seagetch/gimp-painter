#!/usr/bin/env python3
"""Run standalone Linux BindingStore new/delete fault and ownership checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'app/painter'
SOURCES = ['binding-store.cpp', 'gimp-painter-error.cpp',
           'tests/test-store-allocation-failures.cpp']
HEADERS = ['binding-store.hpp', 'object-ref.hpp', 'resources.hpp', 'boundary.hpp',
           'gimp-painter-error.h', 'gimp-painter-visibility.h']


def run(build, sanitize):
    report = {'status': 'FAIL',
              'scope': 'Standalone Linux C++ allocation failure and exact pointer release; '
                       'no GLib fatal-OOM injection, full GIMP, or platform-wide claim',
              'sanitizers': ['address', 'undefined'] if sanitize else [],
              'leak_check': 'not run; explicit tracked new/delete and fixture String/g_free',
              'commands': [], 'output': [],
              'compiled_sources': ['app/painter/' + name for name in SOURCES]}
    paths = [MODULE / name for name in SOURCES + HEADERS] + [Path(__file__).resolve()]
    report['source_sha256'] = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)}

    def execute(argv, env=None):
        argv = list(map(str, argv))
        try:
            result = subprocess.run(argv, text=True, capture_output=True, env=env, timeout=120)
        except subprocess.TimeoutExpired as error:
            def text(value):
                return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
            report['commands'].append({'argv': argv, 'exit_code': None, 'timed_out': True,
                                       'stdout': text(error.stdout), 'stderr': text(error.stderr)})
            raise RuntimeError('120-second timeout: ' + shlex.join(argv)) from error
        report['commands'].append({'argv': argv, 'exit_code': result.returncode,
                                   'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise RuntimeError(shlex.join(argv) + '\n' + result.stdout + result.stderr)
        return result.stdout

    try:
        if platform.system() != 'Linux':
            raise RuntimeError('This harness requires Linux and GNU-compatible linker wrapping')
        cxx = shlex.split(os.environ.get('CXX', 'c++'))
        cflags = shlex.split(execute(['pkg-config', '--cflags', 'gobject-2.0']))
        libs = shlex.split(execute(['pkg-config', '--libs', 'gobject-2.0']))
        flags = ['-std=c++14', '-O0', '-g', '-Wall', '-Wextra', '-Werror',
                 '-fno-rtti', '-fno-lto', '-fvisibility=hidden']
        if sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        objects = []
        for name in SOURCES:
            output = build / (Path(name).name + '.o')
            execute(cxx + flags + cflags + ['-I' + str(MODULE), '-c', MODULE / name,
                                          '-o', output])
            objects.append(output)
        executable = build / 'test-store-allocation-failures'
        wrappers = ['-Wl,--wrap=' + name for name in ('_Znwm', '_ZdlPv', '_ZdlPvm', 'g_free')]
        execute(cxx + flags + objects + wrappers + libs + ['-pthread', '-o', executable])
        report['executable_sha256'] = hashlib.sha256(executable.read_bytes()).hexdigest()
        environment = os.environ.copy()
        if sanitize:
            environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
            environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
        report['runtime_environment'] = {key: environment[key] for key in
            ('ASAN_OPTIONS', 'UBSAN_OPTIONS') if key in environment}
        output = execute([executable], env=environment)
        report['output'] = [output]
        print(output, end='')
        report['status'] = 'PASS'
    except (OSError, RuntimeError) as error:
        report['error'] = str(error)
        print(str(error), file=sys.stderr)
    report['source_sha256_after'] = {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)}
    report['source_unchanged'] = report['source_sha256'] == report['source_sha256_after']
    if not report['source_unchanged']:
        report['status'] = 'FAIL'
        report['error'] = 'Source changed during compilation or execution'
        print(report['error'], file=sys.stderr)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
        report = run(args.build_dir.resolve(), args.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='painter-store-allocation-') as directory:
            report = run(Path(directory), args.sanitize)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())

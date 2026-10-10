#!/usr/bin/env python3
"""Build the test-only BindingStore resolution counter and scoped-borrow cases."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'app/painter'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.build_dir.mkdir(parents=True, exist_ok=True)
    commands = []
    outputs = []

    def run(argv, env=None):
        argv = list(map(str, argv))
        result = subprocess.run(argv, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, env=env)
        commands.append(argv)
        outputs.append(result.stdout)
        if result.returncode:
            raise RuntimeError(shlex.join(argv) + '\n' + result.stdout)
        return result.stdout

    observed = args.build_dir.resolve() / 'observed-binding-store.cpp'
    generator = MODULE / 'tests/observe-binding-store.py'
    run(['python3', generator, MODULE / 'binding-store.cpp', observed])
    sources = [observed, MODULE / 'gimp-painter-error.cpp',
               MODULE / 'tests/binding-store-observation.cpp',
               MODULE / 'tests/test-qdata-operations.cpp']
    cflags = shlex.split(run(['pkg-config', '--cflags', 'gobject-2.0']))
    libs = shlex.split(run(['pkg-config', '--libs', 'gobject-2.0']))
    flags = ['-std=c++14', '-fno-rtti', '-fvisibility=hidden', '-Wall', '-Wextra', '-Werror', '-g']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    executable = args.build_dir.resolve() / 'painter-qdata-operations'
    run(shlex.split(os.environ.get('CXX', 'c++')) + flags + cflags +
        ['-I' + str(MODULE), '-I' + str(MODULE / 'tests')] + sources + libs +
        ['-pthread', '-o', executable])
    environment = os.environ.copy()
    if args.sanitize:
        environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    output = run([executable], env=environment)
    print(output, end='')
    tracked = sources[1:] + [generator, MODULE / 'binding-store.cpp',
        MODULE / 'tests/binding-store-observation.hpp', MODULE / 'binding-store.hpp',
        MODULE / 'object-ref.hpp', MODULE / 'boundary.hpp', MODULE / 'gimp-painter-error.h',
        MODULE / 'gimp-painter-visibility.h', Path(__file__).resolve()]
    report = {'status': 'PASS', 'scope': 'actual resolution counters with synthetic operation admission',
              'sanitizers': ['address', 'undefined'] if args.sanitize else [], 'leak_check': 'not run',
              'commands': commands, 'output': outputs,
              'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in sorted(set(tracked))}}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()

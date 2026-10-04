#!/usr/bin/env python3
"""Compile/run isolated XCF storage in the repository's C++14 dialect.

Caller holds /workspace/shared/gimp-painter-build.lock and sources the documented
Debian dependency environment. These tests do not enable the native transport.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--sanitizers', action='store_true')
parser.add_argument('--detect-leaks', action='store_true', help='Opt in only where LeakSanitizer is supported')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
output = build / ('xcf-storage-sanitizers' if args.sanitizers else 'xcf-storage-isolated')
output.mkdir(exist_ok=True)
entries = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in entries if Path(e['file']).name == 'painter-xcf-load.cpp')
compiler = shlex.split(entry['command'])[0]
flags = ['-std=c++14', '-fexceptions', '-g', '-O1' if args.sanitizers else '-O2', '-Wall', '-Wextra', '-Werror']
flags += ['-frtti', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer'] if args.sanitizers else ['-fno-rtti']
package_flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'gio-2.0'], text=True))
sources = ['app/xcf/painter-xcf-storage.cpp', 'app/xcf/painter-xcf-storage.hpp',
           'app/xcf/painter-xcf-multipart.cpp', 'app/xcf/painter-xcf-multipart.hpp',
           'app/xcf/tests/test-painter-xcf-storage.cpp', 'app/xcf/tests/test-painter-xcf-multipart.cpp',
           'app/painter/bytes.hpp',
           'migration/tests/run_painter_xcf_storage_tests.py']
def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
hashes = {source: digest(root / source) for source in sources}
report = {'scope': 'Isolated C++14 storage/multipart sources and tests; this runner does not exercise native Save/Open',
          'sanitizers': args.sanitizers, 'leak_detection': bool(args.sanitizers and args.detect_leaks), 'source_sha256': hashes,
          'commands': [], 'results': {}}
for name in ['storage', 'multipart']:
    executable = output / ('painter-xcf-' + name)
    command = [compiler, *flags, str(root / 'app/xcf/painter-xcf-storage.cpp')]
    if name == 'multipart':
        command.append(str(root / 'app/xcf/painter-xcf-multipart.cpp'))
    command += [str(root / ('app/xcf/tests/test-painter-xcf-' + name + '.cpp')), '-o', str(executable), *package_flags]
    report['commands'].append(command)
    result = subprocess.run(command, cwd=build, text=True, capture_output=True)
    report['results'][name + '-compile'] = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    env = dict(os.environ)
    if args.sanitizers:
        env.update(ASAN_OPTIONS=f'detect_leaks={int(args.detect_leaks)}:halt_on_error=1:abort_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(executable)], cwd=build, text=True, capture_output=True, env=env)
    report['results'][name] = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                             'executable_sha256': digest(executable)}
    report['changed_during_run'] = [source for source in sources if digest(root / source) != hashes[source]]
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(result.stdout)
    if result.returncode or report['changed_during_run']:
        print(result.stderr)
        raise SystemExit(result.returncode or 1)

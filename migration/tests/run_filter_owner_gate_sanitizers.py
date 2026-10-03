#!/usr/bin/env python3
"""Instrument owner gates and the deterministic real-spool completion race."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--lock', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    common = [ROOT / ('app/painter/' + name + '.cpp') for name in
              ('filter-raster', 'filter-lifetime', 'filter-scheduler')]
    spool = ROOT / 'app/painter/filter-spool.cpp'
    observer = ROOT / 'app/painter/tests/observe-filter-spool.py'
    cases = ['filter-owner-gates', 'filter-import-completion']
    tests = [ROOT / ('app/painter/tests/test-' + name + '.cpp') for name in cases]
    inputs = sorted({*common, spool, observer, Path(__file__).resolve(), *tests,
                     *(ROOT / 'app/painter').glob('*.hpp')})
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    report = {'status': 'incomplete', 'scope': 'Independent scheduler/raster/spool/lifetime and two test harnesses; GLib/system libraries ordinary; LSan off',
              'source_sha256_before': {str(p.relative_to(ROOT)): sha(p) for p in inputs},
              'results': []}
    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        observed = output / 'observed-filter-spool.cpp'
        subprocess.run(['python3', str(observer), str(spool), str(observed)], check=True)
        report['observed_source_sha256'] = sha(observed)
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        for name, test in zip(cases, tests):
            executable = output / ('painter-' + name)
            command = ['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-g', '-O1', '-pthread',
                       '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-frtti',
                       '-I' + str(ROOT / 'app/painter'), *map(str, common),
                       str(observed if name == 'filter-import-completion' else spool), str(test),
                       *flags, '-o', str(executable)]
            subprocess.run(command, check=True)
            result = subprocess.run([str(executable)], env=environment, text=True, capture_output=True, timeout=60)
            report['results'].append({'name': name, 'command': command, 'exit_code': result.returncode,
                                      'executable_sha256': sha(executable), 'stdout': result.stdout, 'stderr': result.stderr})
            print(name, result.returncode, flush=True)
        report['source_sha256_after'] = {str(p.relative_to(ROOT)): sha(p) for p in inputs}
        report['changed_inputs'] = [p for p in report['source_sha256_before']
                                    if report['source_sha256_before'][p] != report['source_sha256_after'][p]]
        report['status'] = 'PASS' if not report['changed_inputs'] and all(r['exit_code'] == 0 for r in report['results']) else 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())

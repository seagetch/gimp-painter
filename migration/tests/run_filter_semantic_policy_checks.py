#!/usr/bin/env python3
"""Compare semantic-policy admission with the immutable accepted Phase A source."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = 'c7b5f74bf520dbaeb46e93a6ce4424e1a08295ca'
HEADER = 'app/painter/filter-procedure.hpp'
SOURCES = [HEADER, 'app/painter/filter-procedure-policy.hpp', 'app/painter/filter-context.hpp',
           'app/painter/tests/test-filter-semantic-policy.cpp']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Report exists; choose a new output')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report = {'status': 'incomplete', 'baseline_commit': BASE, 'sanitized': args.sanitize,
              'scope': 'Pure C++ request policy, not full GIMP or dependency instrumentation', 'commands': []}
    before = {name: sha((ROOT / name).read_bytes()) for name in SOURCES}
    original = subprocess.check_output(['git', 'show', BASE + ':' + HEADER], cwd=ROOT)
    report['baseline_source_sha256'] = sha(original)
    adapted = original.decode().replace('GIMP_PAINTER_FILTER_PROCEDURE_HPP', 'GIMP_PAINTER_BASELINE_FILTER_PROCEDURE_HPP')
    adapted = adapted.replace('namespace GimpPainter {', 'namespace BaselineFilterPolicy {\nusing GimpPainter::FilterSelectionRegion;', 1)
    with tempfile.TemporaryDirectory(prefix='filter-policy-', dir='/workspace/shared') as temporary:
        directory = Path(temporary)
        (directory / 'baseline-filter-procedure.hpp').write_text(adapted)
        executable = directory / 'test-filter-semantic-policy'
        command = ['g++', '-std=c++14', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
                   '-I' + str(ROOT / 'app/painter'), '-I' + str(directory), str(ROOT / SOURCES[-1]), '-o', str(executable)]
        if args.sanitize:
            command += ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
        env = dict(os.environ)
        if args.sanitize:
            env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
        logs = []
        with Path('/workspace/shared/gimp-painter-build.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            for name, command in [('compile', command), ('run', [str(executable)])]:
                result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
                report['commands'].append({'name': name, 'command': command, 'exit_code': result.returncode})
                logs.append(result.stdout + result.stderr)
                if result.returncode:
                    break
            if executable.exists():
                report['binary_sha256'] = sha(executable.read_bytes())
        report['source_sha256'] = before
        report['sources_unchanged'] = before == {name: sha((ROOT / name).read_bytes()) for name in SOURCES}
        report['status'] = 'PASS' if result.returncode == 0 and report['sources_unchanged'] else 'FAIL'
        args.output.with_suffix('.log').write_text(''.join(logs))
        report['log_sha256'] = sha(args.output.with_suffix('.log').read_bytes())
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], ''.join(logs).strip())
        return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())

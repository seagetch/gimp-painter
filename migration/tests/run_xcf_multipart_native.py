#!/usr/bin/env python3
"""Run native XCF acceptance serially with source seals and compact evidence.

Caller sources the pinned Debian environment and holds the shared build lock.
The native harness initializes GIMP without a GUI; no X server is needed. No raw Meson environment or inherited process data is saved.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('build', type=Path)
p.add_argument('--report', type=Path, required=True)
p.add_argument('--test-path')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
targets = ['xcf', 'painter-xcf-open', 'painter-xcf-adversarial', 'painter-provenance', 'painter-xcf-roundtrip']
if a.test_path:
    targets = ['painter-xcf-roundtrip']
paths = set()
for directory, patterns in {'app/xcf': ['*.c', '*.h', '*.cpp', '*.hpp'],
                             'app/painter': ['*.cpp', '*.hpp']}.items():
    for pattern in patterns:
        paths.update(str(path.relative_to(root)) for path in (root / directory).glob(pattern))
paths.update(['app/core/gimp-painter-provenance.cpp', 'app/core/gimp-painter-provenance.h',
              'app/core/gimpfilterlayer.cpp', 'app/core/gimpfilterlayer.h', 'app/core/gimpfilterlayer-arguments.hpp',
              'app/tests/test-painter-xcf-argument-resources.cpp', 'app/core/gimpclonelayer.cpp',
              'app/core/gimpclonelayer.h', 'app/core/gimpimage-duplicate.c',
              'migration/tests/run_xcf_multipart_native.py'])
paths.update(str(path.relative_to(root)) for path in (root / 'app/tests').glob('test-painter-xcf*.inc'))
paths.update('app/tests/test-' + target + '.c' for target in targets)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
source = {name: sha(root / name) for name in sorted(paths)}
report = {'scope': 'Fresh serial native XCF tests; large resource case separately measured',
          'source_sha256': source, 'results': {}, 'commands': []}
env = dict(os.environ)
env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root), GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
           GIMP_TESTING_PLUGINDIRS=str(build / 'plug-ins/common'), UI_TEST='yes', GSETTINGS_BACKEND='memory')
for target in targets:
    exe = build / 'app/tests' / target
    command = [str(exe)] + (['-p', a.test_path] if a.test_path else [])
    report['commands'].append(command)
    started = time.monotonic()
    result = subprocess.run(command, cwd=build, env=env, capture_output=True, text=True)
    tap = [line for line in result.stdout.splitlines() if re.match(r'^(?:ok |not ok |1\.\.|Bail out|# (?:multipart |owner pass|opaque native|Actual temporary))', line)]
    diagnostics = [line for line in result.stderr.splitlines() if any(word in line for word in ['ERROR:', 'AddressSanitizer', 'runtime error:', 'assertion'])]
    report['results'][target] = {'exit_code': result.returncode, 'elapsed_seconds': time.monotonic() - started,
                                 'executable_sha256': sha(exe), 'tap': tap, 'diagnostics': diagnostics,
                                 'stdout_sha256': hashlib.sha256(result.stdout.encode()).hexdigest(),
                                 'stderr_sha256': hashlib.sha256(result.stderr.encode()).hexdigest(),
                                 'passed': sum(line.startswith('ok ') and '# SKIP' not in line for line in tap),
                                 'skipped': sum('# SKIP' in line for line in tap)}
    report['changed_during_run'] = [name for name, digest in source.items() if sha(root / name) != digest]
    a.report.write_text(json.dumps(report, indent=2) + '\n')
    print(target, result.returncode, report['results'][target]['passed'], flush=True)
    if result.returncode or report['changed_during_run']:
        print('\n'.join(tap[-12:] + diagnostics))
        raise SystemExit(1)

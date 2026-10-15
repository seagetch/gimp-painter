#!/usr/bin/env python3
"""Validate all 09.003 source duties and execute the bounded Surface read child.

Compiles current production sources, not an extracted/reimplemented read model.
No native GIMP source/build tree is modified. Broader 09.003 remains open.
"""
import argparse
import csv
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'migration/contracts/drawable-buffer-read.json'
SURFACE = 'app/paint/painter-mypaint-surface/gegl-surface.cpp'
TEST = 'app/paint/painter-mypaint-surface/tests/buffer-read.cpp'
HEADER = 'app/paint/painter-mypaint-surface/buffer-read.hpp'
INPUTS = [HEADER, SURFACE, TEST,
          'app/paint/painter-mypaint-surface/tests/coordinate-boundaries.cpp',
          'app/paint/painter-mypaint-surface/gegl-surface.hpp',
          'app/paint/painter-mypaint-surface/legacy-pixel-modes.hpp',
          'app/paint/painter-mypaint-surface/gray-alpha-pixels.hpp',
          'app/paint/painter-mypaint-surface/native-pixels.hpp',
          'app/paint/painter-mypaint/surface.hpp',
          'app/painter/object-ref.hpp', 'app/painter/boundary.hpp',
          'app/painter/gimp-painter-error.h', 'app/painter/gimp-painter-visibility.h',
          str(MANIFEST.relative_to(ROOT)), 'tools/check_drawable_buffer_read.py']


def require(value, message):
    if not value:
        raise ValueError(message)


def validate(manifest):
    require(manifest['task'] == '09.003/bounded-surface-read', 'Wrong child task')
    require(manifest['parent_task'] == '09.003' and manifest['parent_complete'] is False,
            'Bounded child must not close parent')
    require(manifest['legacy_commit'] == 'afa43fae3e920210146abed514f136fd49f671b5', 'Wrong legacy revision')
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        rows = {r['work_id']: r for r in csv.DictReader(stream, delimiter='\t') if r['wbs_task'] == '09.003'}
    duties = manifest['duties']
    require(len(duties) == 48 and len({d['work_id'] for d in duties}) == 48, 'Missing/duplicate source duty')
    require(set(rows) == {d['work_id'] for d in duties}, 'Wrong duty set')
    sources = {}
    for duty in duties:
        row = rows[duty['work_id']]
        for key in ('source_id', 'path', 'source_blob', 'source_range', 'source_sha256'):
            require(duty[key] == row[key], 'Source identity changed: ' + key)
        if duty['path'] not in sources:
            sources[duty['path']] = subprocess.check_output(['git', 'show', manifest['legacy_commit'] + ':' + duty['path']], cwd=ROOT)
            blob = subprocess.check_output(['git', 'rev-parse', manifest['legacy_commit'] + ':' + duty['path']], cwd=ROOT, text=True).strip()
            require(blob == duty['source_blob'], 'Wrong pinned source blob')
        require(hashlib.sha256(sources[duty['path']]).hexdigest() == duty['blob_sha256'], 'Source bytes changed')
        require(bool(duty['source_duty']) and bool(duty['coverage']), 'Missing duty disposition')
        require('09.003' in duty['remaining_tasks'], 'Parent duty improperly closed')
        if duty['path'] == 'app/core/gimpimage-contiguous-region.c':
            require(duty['scope'] == 'unimplemented-fill-path', 'Fill equivalence not proven by Surface test')
            require('27.004/gegl-bounded-search' in duty['remaining_tasks'], 'Dropped bounded fill work')
            require(bool(duty['changed_evidence']), 'Missing fill hunk evidence')
    return len(duties)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--prove-baseline', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    duties = validate(manifest)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'gegl-0.4'], text=True))
    env = dict(os.environ, GEGL_USE_OPENCL='no', ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    compile_flags = ['-std=c++14', '-Wall', '-Wextra', '-Werror', '-g', '-O1', '-I' + str(ROOT / 'app'),
                     '-I' + str(ROOT / 'app/paint/painter-mypaint-surface')]
    if args.sanitize:
        compile_flags += ['-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    report = {'task': manifest['task'], 'parent_complete': False, 'source_duties': duties,
              'sanitized': args.sanitize, 'scope': 'Current production Surface and helper compiled standalone against real GEGL/babl; no full GIMP or cross-platform claim',
              'limitations': ['Uninstrumented dependency libraries', 'LeakSanitizer disabled', 'TempBuf, fill, clone/filter paths and broader parent obligations remain open'],
              'source_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in INPUTS},
              'gegl': subprocess.check_output(['pkg-config', '--modversion', 'gegl-0.4'], text=True).strip(),
              'babl': subprocess.check_output(['pkg-config', '--modversion', 'babl-0.1'], text=True).strip(),
              'compiler': subprocess.check_output([os.environ.get('CXX', 'c++'), '--version'], text=True).splitlines()[0],
              'compile_flags': compile_flags, 'runs': []}
    with tempfile.TemporaryDirectory(prefix='painter-buffer-read-') as tmp:
        tmp = Path(tmp)
        for name, test in [('buffer-read', TEST), ('coordinates', 'app/paint/painter-mypaint-surface/tests/coordinate-boundaries.cpp')]:
            executable = tmp / name
            command = [os.environ.get('CXX', 'c++'), *compile_flags, str(ROOT / test), str(ROOT / SURFACE), *flags, '-o', str(executable)]
            subprocess.run(command, check=True, cwd=ROOT, env=env)
            result = subprocess.run([str(executable)], capture_output=True, text=True, env=env)
            require(result.returncode == 0, result.stdout + result.stderr)
            passed = len(re.findall(r'^ok [0-9]+ /', result.stdout, re.M))
            require(passed == (8 if name == 'buffer-read' else 5), 'Incomplete executable test groups')
            report['runs'].append({'name': name, 'passed_groups': passed, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        if args.prove_baseline:
            require(args.sanitize, 'Baseline UB proof requires --sanitize')
            old = subprocess.check_output(['git', 'show', manifest['regression_baseline'] + ':' + SURFACE], cwd=ROOT)
            baseline = tmp / 'baseline.cpp'
            baseline.write_bytes(old)
            executable = tmp / 'baseline'
            subprocess.run([os.environ.get('CXX', 'c++'), *compile_flags, str(ROOT / TEST), str(baseline), *flags, '-o', str(executable)], check=True, cwd=ROOT, env=env)
            result = subprocess.run([str(executable), '-p', '/buffer-read/surface-extreme-selection-offsets'], capture_output=True, text=True, env=env)
            require(result.returncode != 0 and 'signed integer overflow' in result.stderr,
                    'Baseline must fail on the diagnosed signed offset overflow: ' + result.stdout + result.stderr)
            report['baseline'] = {'revision': manifest['regression_baseline'], 'source_sha256': hashlib.sha256(old).hexdigest(),
                                  'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'task': report['task'], 'duties': duties, 'test_groups': sum(run['passed_groups'] for run in report['runs']), 'sanitize': args.sanitize,
                      'baseline_failure_proven': 'baseline' in report, 'parent_complete': False}))


if __name__ == '__main__':
    main()

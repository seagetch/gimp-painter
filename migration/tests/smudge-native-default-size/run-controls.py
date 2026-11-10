#!/usr/bin/env python3
"""Revert each of the two gates separately; require red, then restore and green.

Run under tools/run-painter-xvfb.py with the dependency environment sourced.
A private build is required. Never run concurrently with another source build.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p = argparse.ArgumentParser(); p.add_argument('build', type=Path); args = p.parse_args()
root = Path(__file__).resolve().parents[3]; build = args.build.resolve(); out = Path(__file__).resolve().parent
exe = build / 'app/tests/gimp-painter-smudge'
env = {**os.environ, 'GIMP_TESTING_ABS_TOP_SRCDIR': str(root),
       'GIMP_TESTING_ABS_TOP_BUILDDIR': str(build), 'GIMP_TESTING_PLUGINDIRS': str(build / 'plug-ins/common'),
       'GIMP3_DIRECTORY': str(root / 'app/tests/gimpdir'), 'GSETTINGS_BACKEND': 'memory'}
records = []
for source, test in [('app/core/gimpdynamicsoutput.c', 'default-dynamics'),
                     ('app/paint/gimppaintoptions.c', 'native-default-size')]:
    path = root / source; fixed = path.read_bytes()
    before = subprocess.check_output(['git', 'show', 'aec6d72965:' + source], cwd=root)
    record = {'source': source, 'test': test, 'fixed_sha256': hashlib.sha256(fixed).hexdigest(),
              'baseline_sha256': hashlib.sha256(before).hexdigest()}
    try:
        path.write_bytes(before)
        with (out / ('control-' + test + '-build.log')).open('wb') as log:
            subprocess.run(['ninja', '-C', str(build), '-j2', 'app/tests/gimp-painter-smudge'], stdout=log, stderr=subprocess.STDOUT, check=True)
        record['mutant_executable_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
        run = subprocess.run([str(exe), '-p', '/painter/smudge/' + test], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / ('control-' + test + '.log')).write_bytes(run.stdout)
        record['mutant_exit'] = run.returncode
        assert run.returncode != 0 and b'assertion failed' in run.stdout, run.stdout[-2000:]
    finally:
        path.write_bytes(fixed)
        with (out / ('restore-' + test + '-build.log')).open('wb') as log:
            subprocess.run(['ninja', '-C', str(build), '-j2', 'app/tests/gimp-painter-smudge'], stdout=log, stderr=subprocess.STDOUT, check=True)
    run = subprocess.run([str(exe), '-p', '/painter/smudge/' + test], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / ('restore-' + test + '.log')).write_bytes(run.stdout)
    record['restored_exit'] = run.returncode
    record['restored_executable_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
    record['source_restored'] = path.read_bytes() == fixed
    records.append(record)
    (out / 'controls.json').write_text(json.dumps({'controls': records}, indent=2) + '\n')
    assert run.returncode == 0 and record['source_restored'], run.stdout[-2000:]
print('Both independent baseline-gate controls fail; exact restorations pass.')

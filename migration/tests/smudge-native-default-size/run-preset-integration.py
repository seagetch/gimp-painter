#!/usr/bin/env python3
"""Check the preceding preset task against the combined Smudge/preset build."""
import argparse, hashlib, json, os, subprocess, time
from pathlib import Path
p = argparse.ArgumentParser(); p.add_argument('build', type=Path); args = p.parse_args()
root = Path(__file__).resolve().parents[3]; build = args.build.resolve(); out = Path(__file__).resolve().parent
env = {**os.environ, 'GIMP_TESTING_ABS_TOP_SRCDIR': str(root),
       'GIMP_TESTING_ABS_TOP_BUILDDIR': str(build), 'GIMP_TESTING_PLUGINDIRS': str(build / 'plug-ins/common'),
       'GIMP3_DIRECTORY': str(root / 'app/tests/gimpdir'), 'GSETTINGS_BACKEND': 'memory',
       'GIMP_TEST_REQUIRE_PORTRAIT': '1'}
results = []
for name, count in [('preset-catalog', 31), ('preset-catalog-ui', 5),
                    ('preset-gui-entries', 6), ('gimp-layer-presets', 12), ('layer-presets-ui', 18)]:
    executable = build / 'app/tests' / name
    start = time.monotonic(); run = subprocess.run([str(executable)], cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    logfile = out / ('integration-' + name + '.log'); logfile.write_bytes(run.stdout)
    passed = sum(line.startswith(b'ok ') for line in run.stdout.splitlines())
    result = {'name': name, 'exit_code': run.returncode, 'passed_cases': passed,
              'duration': time.monotonic() - start, 'log': logfile.name,
              'log_sha256': hashlib.sha256(run.stdout).hexdigest(),
              'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()}
    results.append(result)
    (out / 'integration-preset-results.json').write_text(json.dumps({'runs': results}, indent=2) + '\n')
    print(name, run.returncode, passed, flush=True)
    assert run.returncode == 0 and passed == count, run.stdout[-2000:]

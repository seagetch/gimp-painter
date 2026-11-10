#!/usr/bin/env python3
"""Same native tests/comparators, directly on an authenticated portrait display.

The normal Meson wrapper starts its own Xvfb/DBus; this runner does neither.
"""
import argparse, hashlib, json, os, subprocess, time
from pathlib import Path
p = argparse.ArgumentParser(); p.add_argument('build', type=Path); args = p.parse_args()
root = Path(__file__).resolve().parents[3]; build = args.build.resolve(); out = Path(__file__).resolve().parent
env = {**os.environ, 'GIMP_TESTING_ABS_TOP_SRCDIR': str(root),
       'GIMP_TESTING_ABS_TOP_BUILDDIR': str(build), 'GIMP_TESTING_PLUGINDIRS': str(build / 'plug-ins/common'),
       'GIMP3_DIRECTORY': str(root / 'app/tests/gimpdir'), 'GSETTINGS_BACKEND': 'memory',
       'GIMP_TEST_REQUIRE_PORTRAIT': '1', 'GIMP_TEST_ONCANVAS_SCREENSHOTS': str(out)}
commands = [(name, [str(build / 'app/tests' / name)]) for name in ['gimp-painter-smudge', 'painter-brush-geometry', 'painter-smudge-ui']]
for route in ['legacy', 'owned']:
    commands.append((route, ['python3', str(root / 'migration/tests/compare_smudge_strokes.py'),
      str(root / 'migration/fixtures/legacy-smudge/pixels.tsv.gz'), str(build / 'app/tests/painter-smudge-trace'), *(['owned'] if route == 'owned' else [])]))
for route in ['raw', 'path', 'boundary']:
    commands.append(('generic-' + route, ['python3', str(root / 'migration/tests/compare_owned_generic.py'), str(build),
      '--family', 'smudge', '--route', route, '--binary', str(build / 'app/tests/painter-smudge-trace'),
      '--report', str(out / ('generic-' + route + '.json'))]))
results = []
for name, command in commands:
    start = time.monotonic(); run = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    logfile = out / ('final-' + name + '.log'); logfile.write_bytes(run.stdout)
    result = {'name': name, 'command': command, 'exit_code': run.returncode, 'duration': time.monotonic() - start,
              'log': logfile.name, 'sha256': hashlib.sha256(run.stdout).hexdigest()}
    if Path(command[0]).is_file(): result['executable_sha256'] = hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()
    results.append(result)
    (out / 'native-results.json').write_text(json.dumps({'runs': results}, indent=2) + '\n')
    print(name, run.returncode, flush=True)
    assert run.returncode == 0, run.stdout[-2000:]

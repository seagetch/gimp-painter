#!/usr/bin/env python3
"""Compare actual generic port APIs against pinned old native entrypoints.

BUILD --report REPORT retains the all-six comparison. Meson selects one case
with --family FAMILY --route ROUTE --binary TARGET and its own report path.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FAMILIES = {'fill-brush': b'BRUSH ', 'smudge': b'SMUDGE '}
ROUTES = ('raw', 'path', 'boundary')


def compare(build, family, route, executable):
    expected = gzip.decompress(
        (ROOT / 'migration/fixtures/legacy-owned-generic' /
         (family + '-' + route + '.tsv.gz')).read_bytes())
    # Use the ordinary tests.c source-fixture copier, inside a fresh writable
    # runtime for each route. Never reuse gimpdir-output as personal input.
    with tempfile.TemporaryDirectory(prefix='painter-owned-' + family + '-' + route + '-') as temporary:
        env = {**os.environ,
               'GIMP_TESTING_ABS_TOP_SRCDIR': str(ROOT),
               'GIMP_TESTING_ABS_TOP_BUILDDIR': str(build),
               'GIMP_TESTING_PLUGINDIRS': str(build / 'plug-ins/common'),
               'GIMP3_DIRECTORY': str(ROOT / 'app/tests/gimpdir'),
               'UI_TEST': 'yes', 'GSETTINGS_BACKEND': 'memory',
               'PAINTER_OWNED_GENERIC_ROUTE': route}
        for variable, directory in (
                ('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'),
                ('XDG_CACHE_HOME', 'cache'), ('XDG_DATA_HOME', 'data'),
                ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime'),
                ('TMPDIR', 'tmp')):
            path = Path(temporary) / directory
            path.mkdir(mode=0o700)
            env[variable] = str(path)
        run = subprocess.run([str(executable)], env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sys.stderr.buffer.write(run.stderr)
    # Keep the captured oracle's normalization and byte-exact acceptance.
    actual = b'\n'.join(line for line in run.stdout.splitlines()
                        if line.startswith(FAMILIES[family]) or
                        line.endswith(b'_CAPTURE_COMPLETE')) + b'\n'
    record = {'family': family, 'route': route, 'exit_code': run.returncode,
              'bytes': len(actual), 'records': len(actual.splitlines()),
              'exact': actual == expected, 'stderr': run.stderr.decode(),
              'sha256': hashlib.sha256(actual).hexdigest(),
              'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()}
    if actual != expected:
        record['differences'] = [
            {'record': str(left.split()[:3]),
             'different_hex_characters': sum(x != y for x, y in zip(left, right))}
            for left, right in zip(expected.splitlines(), actual.splitlines())
            if left != right]
    return record


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--family', choices=FAMILIES)
    parser.add_argument('--route', choices=ROUTES)
    parser.add_argument('--binary', type=Path)
    args = parser.parse_args(argv)
    selected = (args.family is not None, args.route is not None, args.binary is not None)
    if any(selected) and not all(selected):
        parser.error('--family, --route and --binary must be supplied together')

    build = args.build.resolve()
    cases = ([(args.family, args.route, args.binary.resolve())] if all(selected) else
             [(family, route, build / 'app/tests' / ('painter-' + family + '-trace'))
              for family in FAMILIES for route in ROUTES])
    runs = []
    for family, route, executable in cases:
        record = compare(build, family, route, executable)
        runs.append(record)
        print(f'{family}/{route}: exit={record["exit_code"]}, exact={record["exact"]}', flush=True)
    args.report.write_text(json.dumps({
        'scope': 'Real generic port entrypoints versus independent pinned old Stroke/Vectors/Boundary fixtures',
        'runs': runs}, indent=2) + '\n')
    return 0 if all(run['exit_code'] == 0 and run['exact'] for run in runs) else 1


if __name__ == '__main__':
    raise SystemExit(main())

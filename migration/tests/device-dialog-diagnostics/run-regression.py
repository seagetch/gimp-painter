#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run the device readiness regression without replacing normal objects.

Use the selected dependency environment. --build-phase works without a display;
--run-phase requires a native GTK display. Both phases hold the shared lock.
Only the listed C units receive ASan/UBSan; other GIMP/C++ and dependencies do not.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASE = '48449764e08a76d123e324ea55a3d75b5ae4fb46'
EVENTS = 'app/display/gimpdisplayshell-tool-events.c'
FLAGS = ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-O1', '-g']
FOCUSED = {
    EVENTS, 'app/core/gimp.c', 'app/widgets/gimpdeviceinfo.c',
    'app/widgets/gimpdeviceinfo-coords.c', 'app/widgets/gimpdevicemanager.c',
    'app/widgets/gimpdevices.c', 'app/tests/test-device-dialog-events.c',
    'app/tests/test-painter-navigation-events.c',
}
TARGETS = ['device-dialog-events', 'painter-navigation-events']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build-installed-filter')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', default=BASE)
    parser.add_argument('--build-phase', action='store_true')
    parser.add_argument('--run-phase', action='store_true')
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    args = parser.parse_args()
    if not (args.build_phase or args.run_phase):
        parser.error('select --build-phase and/or --run-phase')
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report_path = out / 'regression.json'
    report = json.loads(report_path.read_text()) if report_path.exists() else {'results': []}
    # A reused directory must never retain an earlier success on failure.
    report['status'] = 'incomplete'
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    report['dependency_instrumentation'] = False
    report['leak_sanitizer'] = False
    report['sanitizer_scope'] = sorted(FOCUSED)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    def publish():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    def command(cmd, label):
        report.setdefault('commands', []).append(cmd)
        result = subprocess.run(cmd, cwd=build, capture_output=True, text=True)
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            publish()
            raise RuntimeError(label + ': ' + str(result.returncode))
        return result.stdout

    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if args.build_phase:
            report['build_started_utc'] = datetime.now(timezone.utc).isoformat()
            report['commands'] = []
            command(['ninja', '-C', str(build), *['app/tests/' + x for x in TARGETS]], 'build-normal')
            entries = {}
            for entry in json.loads((build / 'compile_commands.json').read_text()):
                path = (Path(entry['directory']) / entry['file']).resolve()
                if path.is_relative_to(ROOT):
                    name = path.relative_to(ROOT).as_posix()
                    if name in FOCUSED:
                        if name in entries:
                            raise RuntimeError('Ambiguous compile entry: ' + name)
                        entries[name] = entry
            if entries.keys() != FOCUSED:
                raise RuntimeError('Missing compile entries')
            inputs = {name: sha(ROOT / name) for name in FOCUSED}
            report['source_sha256'] = inputs
            report['baseline_commit'] = args.baseline
            baseline = out / 'baseline-tool-events.c'
            baseline.write_bytes(subprocess.check_output(['git', 'show', args.baseline + ':' + EVENTS], cwd=ROOT))
            report['baseline_source_sha256'] = sha(baseline)
            for variant in ('baseline', 'sanitized'):
                replacements = {}
                selected = {EVENTS} if variant == 'baseline' else FOCUSED
                for source in sorted(selected):
                    entry = entries[source]
                    cmd = shlex.split(entry['command'])
                    filtered, index = [], 0
                    while index < len(cmd):
                        if cmd[index] in ('-o', '-MF', '-MT', '-MQ'):
                            index += 2
                            continue
                        if cmd[index] in ('-MD', '-MMD'):
                            index += 1
                            continue
                        filtered.append(cmd[index])
                        index += 1
                    if variant == 'baseline':
                        filtered = [str(baseline) if x == entry['file'] else x for x in filtered]
                    obj = out / (variant + '-' + source.replace('/', '_') + '.o')
                    command(filtered + (FLAGS if variant == 'sanitized' else []) + ['-o', str(obj)],
                            'compile-' + obj.name)
                    replacements[entry['output']] = str(obj)
                absolute = {str((build / old).resolve()): new for old, new in replacements.items()}
                archives = {}
                for target in (['device-dialog-events'] if variant == 'baseline' else TARGETS):
                    link = shlex.split(subprocess.check_output(['ninja', '-t', 'commands', 'app/tests/' + target],
                                                               cwd=build, text=True).strip().splitlines()[-1])
                    link[link.index('-o') + 1] = str(out / (variant + '-' + target))
                    for archive in sorted({x for x in link if x.endswith('.a')}):
                        if archive not in archives:
                            members = [str((build / member).resolve()) for member in
                                       subprocess.check_output(['ar', 't', archive], cwd=build, text=True).splitlines()]
                            archives[archive] = None
                            if any(member in absolute for member in members):
                                private_archive = out / (variant + '-' + archive.replace('/', '_'))
                                private_archive.unlink(missing_ok=True)
                                command(['ar', 'crsT', str(private_archive), *[absolute.get(x, x) for x in members]],
                                        'archive-' + private_archive.name)
                                archives[archive] = str(private_archive)
                        if archives[archive]:
                            link = [archives[archive] if x == archive else x for x in link]
                    link = [replacements.get(x, x) for x in link]
                    command(link + (FLAGS if variant == 'sanitized' else []), 'link-' + variant + '-' + target)
            report['binaries'] = {str(path): sha(path) for path in [
                *[build / 'app/tests' / x for x in TARGETS],
                out / 'baseline-device-dialog-events',
                *[out / ('sanitized-' + x) for x in TARGETS]]}
            if inputs != {name: sha(ROOT / name) for name in FOCUSED}:
                raise RuntimeError('Sources changed during build')
            report['build_completed_utc'] = datetime.now(timezone.utc).isoformat()
            publish()
        if args.run_phase:
            for path, digest in report['binaries'].items():
                if sha(path) != digest:
                    raise RuntimeError('Changed binary: ' + path)
            specs = {x['name']: x for x in json.loads((build / 'meson-info/intro-tests.json').read_text())}
            runs = [('baseline', 'device-dialog-events', out / 'baseline-device-dialog-events')]
            runs += [('normal', x, build / 'app/tests' / x) for x in TARGETS]
            runs += [('sanitized', x, out / ('sanitized-' + x)) for x in TARGETS]
            report['results'] = []
            for variant, target, binary in runs:
                env = os.environ.copy()
                previous = env.get('LD_LIBRARY_PATH', '')
                env.update(specs[target]['env'])
                env['LD_LIBRARY_PATH'] += os.pathsep + previous
                env.update(GSETTINGS_BACKEND='memory',
                           ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                start = datetime.now(timezone.utc).isoformat()
                result = subprocess.run([str(binary)], cwd=build, env=env, capture_output=True, text=True, timeout=120)
                output = result.stdout + result.stderr
                name = variant + '-' + target
                (out / (name + '.log')).write_text(output)
                if variant == 'baseline':
                    passed = result.returncode != 0 and result.returncode != 77 and \
                             "gimp_device_info_get_device: assertion 'GIMP_IS_DEVICE_INFO (info)' failed" in output
                else:
                    passed = result.returncode == 0 and 'CRITICAL' not in output and \
                             'runtime error:' not in output and 'ERROR: AddressSanitizer' not in output
                report['results'].append(dict(name=name, started_utc=start,
                    ended_utc=datetime.now(timezone.utc).isoformat(), exit_code=result.returncode,
                    expected_baseline_failure=variant == 'baseline', passed=passed, log=name + '.log'))
                publish()
                print(name, result.returncode, 'PASS' if passed else 'FAIL', flush=True)
                if not passed:
                    raise RuntimeError('Unexpected result: ' + name)
            report['status'] = 'passed'
            publish()


if __name__ == '__main__':
    main()

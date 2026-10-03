#!/usr/bin/env python3
"""Snapshot exact XCF/cancel verification inputs before compilation, then verify.

Run with the local build environment loaded and the shared build lock held.
Historical reports are immutable inputs; new results use distinct filenames.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build-debian13'
OUT = ROOT / 'migration/tests'
HISTORICAL = OUT / 'xcf-reconstructed-adversarial-sanitizers.json'
REPORT = OUT / 'xcf-reconstructed-sealed-verification.json'
SOURCES = OUT / 'xcf-reconstructed-sealed-inputs.tar.gz'
PARTIAL = OUT / 'xcf-reconstructed-historical-matching-inputs.tar.gz'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def archive(path, payloads, metadata):
    with tarfile.open(path, 'w:gz') as tf:
        for name, data in sorted({**payloads, 'SNAPSHOT.json': (json.dumps(metadata, indent=2) + '\n').encode()}.items()):
            member = tarfile.TarInfo(name)
            member.size = len(data)
            member.mode = 0o444
            member.mtime = 0
            tf.addfile(member, io.BytesIO(data))
    with tarfile.open(path, 'r:gz') as tf:
        for name, data in payloads.items():
            assert sha(tf.extractfile(name).read()) == sha(data), name
    return {'path': path.relative_to(ROOT).as_posix(), 'sha256': sha(path.read_bytes()), 'bytes': path.stat().st_size}


historical = json.loads(HISTORICAL.read_text())
recorded = historical['sources_sha256']
matching, drift = {}, []
for name, expected in recorded.items():
    data = (ROOT / name).read_bytes()
    if sha(data) == expected:
        matching[name] = data
    else:
        drift.append({'path': name, 'recorded_sha256': expected, 'current_sha256': sha(data)})
partial_metadata = {'historical_report': HISTORICAL.relative_to(ROOT).as_posix(),
                    'historical_report_sha256': sha(HISTORICAL.read_bytes()),
                    'recorded_input_count': len(recorded), 'matching_input_count': len(matching),
                    'missing_historical_inputs': drift,
                    'complete_historical_source_snapshot': not drift}
drift_path = OUT / 'xcf-reconstructed-historical-input-drift.json'
if PARTIAL.exists() and drift_path.exists():
    # A later runner fix must not replace the already captured historical bytes.
    prior = json.loads(drift_path.read_text())
    partial_archive = prior['archive']
    assert sha(PARTIAL.read_bytes()) == partial_archive['sha256']
    drift = prior['missing_historical_inputs']
else:
    partial_archive = archive(PARTIAL, matching, partial_metadata)
    drift_path.write_text(json.dumps({**partial_metadata, 'archive': partial_archive}, indent=2) + '\n')

# Quote-included repository headers are captured recursively, in addition to
# every measured unit/header. System dependency headers remain environmental.
names = set(recorded) | {'app/tests/test-gimp-filter-layer.c', 'app/tests/test-gimp-filter-layout.cpp',
                         'app/tests/tests.c', 'app/tests/meson.build',
                         'migration/tests/seal_xcf_recovery_evidence.py'}
queue = list(names)
while queue:
    name = queue.pop()
    path = ROOT / name
    if path.suffix not in {'.c', '.cc', '.cpp', '.h', '.hpp', '.inc'}:
        continue
    for include in re.findall(rb'^\s*#\s*include\s*"([^"\n]+)"', path.read_bytes(), re.M):
        inc = include.decode()
        for base in (path.parent, ROOT / 'app', ROOT, BUILD):
            candidate = (base / inc).resolve()
            if candidate.is_file() and candidate.is_relative_to(ROOT):
                resolved = candidate.relative_to(ROOT).as_posix()
                if resolved not in names:
                    names.add(resolved)
                    queue.append(resolved)
                break
payloads = {name: (ROOT / name).read_bytes() for name in names}
hashes = {name: sha(data) for name, data in payloads.items()}
snapshot_metadata = {'source_reported_inputs': len(recorded),
                     'instrumented_xcf_units': historical['instrumented_sources'],
                     'rtti_only_compatibility_units': historical['rtti_compatibility_only_sources'],
                     'additional_instrumented_cancel_harness': ['app/tests/test-gimp-filter-layer.c', 'app/tests/test-gimp-filter-layout.cpp'],
                     'files_sha256': hashes,
                     'capture': 'All bytes captured and archive verified before compilation; later drift invalidates this verification.'}
source_archive = archive(SOURCES, payloads, snapshot_metadata)
report = {'snapshot': source_archive, 'historical_matching_snapshot': partial_archive,
          'historical_drift': drift, 'source_sha256': hashes,
          'normal': {}, 'sanitizers': {}, 'cancel': [], 'passed': False}
REPORT.write_text(json.dumps(report, indent=2) + '\n')
print('Captured', len(hashes), 'exact source/header inputs;', source_archive, flush=True)


def unchanged():
    return [name for name, expected in hashes.items() if sha((ROOT / name).read_bytes()) != expected]


def publish():
    report['changed_after_capture'] = unchanged()
    REPORT.write_text(json.dumps(report, indent=2) + '\n')


targets = ['xcf', 'painter-xcf-open', 'painter-xcf-adversarial', 'painter-xcf-roundtrip', 'painter-provenance', 'gimp-filter-layer']
subprocess.run(['ninja', '-C', str(BUILD), '-j4', *['app/tests/' + target for target in targets]], cwd=ROOT, check=True)
normal_command = ['meson', 'test', '-C', str(BUILD), '--no-rebuild', '--print-errorlogs', '--logbase', 'xcf-reconstructed-sealed-normal', *targets[:-1]]
normal = subprocess.run(normal_command, cwd=ROOT, capture_output=True, text=True)
records = [json.loads(line) for line in (BUILD / 'meson-logs/xcf-reconstructed-sealed-normal.json').read_text().splitlines()]
report['normal'] = {'command': normal_command, 'exit_code': normal.returncode,
                    'records': [{key: row.get(key) for key in ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')} for row in records]}
publish()
if normal.returncode or len(records) != 5 or unchanged():
    raise RuntimeError('Normal checkpoint failed or captured source changed')

san_report = OUT / 'xcf-reconstructed-sealed-sanitizers.json'
san_command = ['python3', 'migration/tests/run_painter_xcf_adversarial_sanitizers.py', str(BUILD), '--report', str(san_report), '--run']
subprocess.run(san_command, cwd=ROOT, check=True)
san = json.loads(san_report.read_text())
assert all(hashes[name] == expected for name, expected in san['sources_sha256'].items())
assert not san['changed_during_build'] and not san['changed_during_run']
assert san['results']['painter-xcf-adversarial']['exit_code'] == 0
assert len(re.findall(r'^ok \d+ ', san['results']['painter-xcf-adversarial']['stdout'], re.M)) == 246
report['sanitizers'] = {'report': san_report.relative_to(ROOT).as_posix(), 'sha256': sha(san_report.read_bytes()),
                        'scope': san['scope'], 'cases_passed': 246}
publish()

# Reuse the exact same instrumented production and RTTI-only archive overlay.
# Only the Filter C/layout harness needs additional compilation; only two
# explicit-cancel tests run, never the full Filter suite or large fixtures.
overlay = BUILD / 'xcf-adversarial-rtti-sanitizers'
entries = json.loads((BUILD / 'compile_commands.json').read_text())
replacements, compile_commands = {}, []
flags = ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-O1']
for source in ('app/tests/test-gimp-filter-layer.c', 'app/tests/test-gimp-filter-layout.cpp'):
    entry = next(entry for entry in entries if (Path(entry['directory']) / entry['file']).resolve() == ROOT / source)
    command, skip = [], False
    for argument in shlex.split(entry['command']):
        if skip:
            skip = False
            continue
        if argument in ('-MF', '-MQ', '-MT'):
            skip = True
            continue
        if argument not in ('-MD', '-MMD'):
            command.append(argument)
    obj = overlay / (Path(source).name + '.o')
    command[command.index('-o') + 1] = str(obj)
    command += flags
    if source.endswith('.cpp'):
        command += ['-frtti']
    subprocess.run(command, cwd=BUILD, check=True)
    compile_commands.append(command)
    replacements[entry['output']] = str(obj)
link = shlex.split(subprocess.check_output(['ninja', '-t', 'commands', 'app/tests/gimp-filter-layer'], cwd=BUILD, text=True).strip().splitlines()[-1])
exe = overlay / 'gimp-filter-layer-explicit-cancel'
link[link.index('-o') + 1] = str(exe)
link = [replacements.get(arg, str(overlay / arg.replace('/', '_')) if arg.endswith('.a') and (overlay / arg.replace('/', '_')).exists() else arg) for arg in link]
link[1:1] = flags
subprocess.run(link, cwd=BUILD, check=True)
report['cancel_harness'] = {'instrumented_sources': list(snapshot_metadata['additional_instrumented_cancel_harness']),
                            'compile_commands': compile_commands, 'link_command': link, 'executable_sha256': sha(exe.read_bytes()),
                            'scope': 'Two instrumented harness units plus the same XCF production overlay; other application/dependency units uninstrumented; LSan disabled'}
env = dict(os.environ)
env.update({'GIMP_TESTING_ABS_TOP_SRCDIR': str(ROOT), 'GIMP_TESTING_ABS_TOP_BUILDDIR': str(BUILD),
            'GIMP_TESTING_PLUGINDIRS': str(BUILD / 'plug-ins/common'), 'UI_TEST': 'yes', 'GSETTINGS_BACKEND': 'memory',
            'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1:abort_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
for name in ('explicit_cancel_preserves_completed_cache', 'explicit_cancel_survives_callback_reentry'):
    command = [str(exe), '-p', '/gimp-filter-layer/' + name]
    result = subprocess.run(command, cwd=BUILD, env=env, capture_output=True, text=True)
    passed = result.returncode == 0 and '1..1' in result.stdout and ('ok 1 /gimp-filter-layer/' + name) in result.stdout
    report['cancel'].append({'name': name, 'command': command, 'exit_code': result.returncode,
                              'passed': passed, 'stdout': result.stdout, 'stderr': result.stderr})
    publish()
    if not passed:
        raise RuntimeError('Focused cancel case failed: ' + name)
report['passed'] = not unchanged()
publish()
if not report['passed']:
    raise RuntimeError('Source changed after capture')
print('Sealed five normal suites, 246 XCF sanitizer cases, and two explicit-cancel sanitizer cases', flush=True)

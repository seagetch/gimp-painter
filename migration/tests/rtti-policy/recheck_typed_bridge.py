#!/usr/bin/env python3
"""Recheck only the corrected isolated typed-bridge fixture, preserving native proof.

The initial run used GInitiallyUnowned, an alias of GObject, as a distinct C++
TypeTraits specialization. That fixture-only compile error is preserved in the
initial report. No foundation/runtime or production compilation is repeated.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from check_painter_rtti_policy import digest, run, succeeded


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--initial-report', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    report = json.loads(args.initial_report.read_text())
    require(report['status'] == 'FAIL', 'Expected initial fixture-only failure')
    require(report['foundation']['status'] == 'PASS', 'Foundation was not proven')
    for flag in ('original_objects_unchanged', 'metadata_unchanged',
                 'production_sources_unchanged', 'core_display_scope_complete'):
        require(report[flag], flag)
    require(not report['summary']['painter_flags_outside_replay_gaps'], 'Unexpected Painter policy gap')
    for path, sha in report['inputs'].items():
        require(digest(path) == sha, 'Changed metadata/checker: ' + path)
    for unit in report['production_units']:
        require(digest(ROOT / unit['source']) == unit['source_sha256'], 'Changed production source')
    for path, sha in report['foundation']['common_source_sha256'].items():
        require(digest(ROOT / path) == sha, 'Changed common source')
    foundation = report['foundation']
    require(foundation['runtime_rerun'], 'This repair expects the fresh-run report')
    require(digest(foundation['runtime']['command'][0]) == foundation['binary_sha256'], 'Changed foundation binary')
    require(digest(foundation['runtime']['log']) == foundation['runtime']['log_sha256'], 'Changed foundation log')
    for result in report['replays']:
        require(succeeded(result), 'Production compilation failed')
        require(digest(result['object']) == result['object_sha256'], 'Changed isolated replay object')
    for path, sha in report['original_object_sha256_before'].items():
        require((digest(path) if Path(path).exists() else None) == sha, 'Changed original object')
    typed = None
    for case in report['controls']['cases']:
        if case['name'] == 'typed-bridge':
            typed = case
        else:
            require(all(result['passed'] for result in case['runs']), 'Another control failed')
            require(digest(ROOT / 'migration/tests/rtti-policy' / (case['name'] + '.cpp')) == case['source_sha256'],
                    'Changed passing fixture')
    require(typed is not None and len(typed['runs']) == 1 and not typed['runs'][0]['passed'], 'Unexpected typed control')
    previous = typed['runs'][0]
    build = Path(previous['directory'])
    targets = json.loads((build / 'meson-info/intro-targets.json').read_text())
    archive = Path(next(target for target in targets if target['name'] == 'apppainter')['filename'][0])
    require(digest(archive) == typed['archive_sha256'], 'Changed common archive')
    work = args.work_dir.resolve()
    require(not work.is_relative_to(build) and not build.is_relative_to(work), 'Work must be separate from build')
    work.mkdir(parents=True, exist_ok=False)
    command = list(previous['command'])
    command[command.index('-o') + 1] = str(work / 'typed-bridge.o')
    result = run(command, build, work / 'typed-bridge-compile.log')
    libs = subprocess.check_output(['pkg-config', '--libs', 'gobject-2.0'], text=True)
    if succeeded(result):
        result['link'] = run([command[0], '-fno-rtti', str(work / 'typed-bridge.o'), str(archive), '-pthread']
                             + shlex.split(libs) + ['-o', str(work / 'typed-bridge')], build, work / 'typed-bridge-link.log')
        if succeeded(result['link']):
            result['runtime'] = run([str(work / 'typed-bridge')], work, work / 'typed-bridge-run.log')
    result['passed'] = succeeded(result) and succeeded(result.get('link', {})) and succeeded(result.get('runtime', {}))
    typed['runs'] = [result]
    typed['source_sha256'] = digest(ROOT / 'migration/tests/rtti-policy/typed-bridge.cpp')
    report['controls']['status'] = 'PASS' if result['passed'] else 'FAIL'
    report['status'] = report['controls']['status']
    report['fixture_correction'] = {
        'initial_report': str(args.initial_report), 'initial_report_sha256': digest(args.initial_report),
        'helper_sha256': digest(__file__),
        'reason': 'Replace the GInitiallyUnowned C typedef alias with a distinct opaque test owner for TypeTraits.',
        'rechecked': 'Only typed-bridge compilation, actual common-archive link and runtime.',
        'reused_verified': 'Exact native metadata, source, foundation binary/log, common archive, all replay objects and passing fixtures.'}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], **report['summary']}, indent=2))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())

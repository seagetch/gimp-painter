#!/usr/bin/env python3
"""Export allowlisted Meson results and enforce the focused UI evidence gate.

Raw Meson logs can include inherited environment. Never copy them into public
artifacts. This report includes only test output and non-environment fields.
PNG geometry and hashes do not replace human inspection of the actual images.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


ROUTES = ('ordinary-tile', 'clone-tile', 'ordinary-tree', 'clone-tree')
SCREENSHOT_VARIANTS = ('', '-bottom')
SKIP = re.compile(r'^(?:ok|not ok)\b[^\n]*#\s*(?:SKIP|TODO)\b', re.I | re.M)


def read_results(path: Path, expected: list[str], expected_cases: int | None = None,
                 case_ids: list[str] | None = None):
    """Fail closed on missing/duplicate registrations, bad results or TAP skips."""
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    safe, errors, seen = [], [], set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('Meson result must be an object')
        name = row.get('name', '')
        # Meson may qualify names with project and suite in multi-suite logs.
        name = name.rsplit(' / ', 1)[-1]
        if name in seen:
            errors.append('Duplicate registration: ' + name)
        seen.add(name)
        item = {key: row.get(key) for key in
                ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}
        output = str(row.get('stdout', '')) + '\n' + str(row.get('stderr', ''))
        if row.get('result') != 'OK' or row.get('returncode') != 0:
            errors.append('Non-passing registration: ' + name)
        if SKIP.search(output):
            errors.append('Skipped/TODO native cases: ' + name)
        if expected_cases is not None:
            cases = re.findall(r'^ok\s+\d+\s+/', output, flags=re.M)
            plans = re.findall(r'^1\.\.(\d+)\s*$', output, flags=re.M)
            if len(cases) != expected_cases or plans != [str(expected_cases)]:
                errors.append(f'Expected {expected_cases} executed cases: ' + name)
        if case_ids is not None:
            actual = re.findall(r'^ok\s+\d+\s+(/\S+)', output, flags=re.M)
            plans = re.findall(r'^1\.\.(\d+)\s*$', output, flags=re.M)
            if (len(actual) != len(case_ids) or set(actual) != set(case_ids) or
                    plans != [str(len(case_ids))]):
                errors.append('Executed case IDs differ from committed case manifest: ' + name)
        safe.append(item)
    if seen != set(expected):
        errors.append(f'Registration mismatch: expected {sorted(expected)}, got {sorted(seen)}')
    return {'passed': not errors, 'errors': errors, 'tests': safe}


def check_pngs(directory: Path, scale: int):
    expected = {f'{route}{variant}-scale{scale}.png'
                for route in ROUTES for variant in SCREENSHOT_VARIANTS}
    observed = {p.name for p in directory.glob('*.png')}
    errors = [] if observed == expected else ['Missing or extra screenshot filenames']
    images = []
    for name in sorted(observed):
        path = directory / name
        if path.is_symlink() or not path.is_file():
            errors.append('Screenshot must be a regular file: ' + name)
            continue
        data = path.read_bytes()
        if len(data) < 24 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
            errors.append('Invalid PNG header: ' + name)
            continue
        width, height = struct.unpack('>II', data[16:24])
        if (width, height) != (1080, 1920):
            errors.append('Wrong physical screenshot dimensions: ' + name)
        images.append({'file': name, 'width': width, 'height': height,
                       'sha256': hashlib.sha256(data).hexdigest(), 'size_bytes': len(data)})
    return {'passed': not errors, 'errors': errors, 'images': images,
            'visual_inspection': 'pending; geometry and hashes are not visual acceptance'}


def diagnostic_report(report, prefix):
    """Route-only evidence is never an acceptance result, even if it passes."""
    report['diagnostic_passed'] = report.pop('passed')
    report.update(diagnostic_only=True, acceptance_eligible=False,
                  selected_prefix=prefix,
                  scope='Isolated route diagnosis only; cannot replace or add to full-suite acceptance')
    report['diagnostic_status'] = ('incomplete_timeout' if any(
        row.get('result') == 'TIMEOUT' for row in report['tests']) else
        'passed_subset' if report['diagnostic_passed'] else 'failed_or_incomplete_subset')
    return report


def read_route_diagnostic(path: Path, expected, case_ids, prefix):
    allowed = [f'/common-layer-native-popup/{route}' for route in ROUTES]
    if prefix not in allowed or expected != ['painter-common-layer-popup'] or not case_ids:
        raise ValueError('Diagnostic prefix requires a common route and its committed case manifest')
    selected = [case for case in case_ids if case.startswith(prefix + '/')]
    if not selected:
        raise ValueError('Diagnostic prefix has no committed cases')
    return diagnostic_report(read_results(path, expected, case_ids=selected), prefix)


def read_baseline_red(path: Path, selected_case: str):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    errors = []
    if len(rows) != 1 or not isinstance(rows[0], dict):
        return {'passed': False, 'errors': ['Expected exactly one baseline registration'], 'tests': []}
    row = rows[0]
    output = str(row.get('stdout', '')) + '\n' + str(row.get('stderr', ''))
    command = row.get('command', [])
    selected = any(command[i:i+2] == ['-p', selected_case] for i in range(len(command)-1))
    carrier = re.search(r"GTK_IS_WINDOW\s*\(\s*popup\s*\)['\"]?\s+should be TRUE", output)
    if row.get('name', '').rsplit(' / ', 1)[-1] != 'painter-common-layer-popup':
        errors.append('Wrong baseline registration')
    if not selected or selected_case not in [f'/common-layer-native-popup/{route}/actual-entry' for route in ROUTES]:
        errors.append('Missing/wrong selected baseline case')
    if row.get('result') != 'FAIL' or type(row.get('returncode')) is not int or row['returncode'] == 0:
        errors.append('Baseline must fail, not pass/skip/timeout')
    if not carrier or 'test-common-layer-popup.cpp' not in output or SKIP.search(output):
        errors.append('Not the intended GTK_IS_WINDOW(popup) carrier assertion')
    return {'passed': not errors, 'errors': errors, 'selected_case': selected_case,
            'scope': 'Expected baseline carrier RED only; not a passing baseline UI test',
            'tests': [{key: row.get(key) for key in
                       ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--expect', nargs='+', required=True)
    parser.add_argument('--cases', type=int)
    parser.add_argument('--case-manifest', type=Path)
    parser.add_argument('--baseline-red-case')
    parser.add_argument('--diagnostic-prefix')
    parser.add_argument('--screenshots', type=Path)
    parser.add_argument('--scale', type=int, choices=[1, 2])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        cases = None
        if args.case_manifest:
            manifest = json.loads(args.case_manifest.read_text())
            if manifest['schema_version'] != 1 or args.expect != [manifest['test']]:
                raise ValueError('Case manifest does not match selected registration')
            cases = manifest['case_ids']
            if not cases or len(cases) != len(set(cases)):
                raise ValueError('Case manifest must have unique, nonempty IDs')
        if args.diagnostic_prefix:
            if args.baseline_red_case or args.screenshots or args.cases is not None:
                raise ValueError('Diagnostic routes cannot use baseline or screenshot acceptance gates')
            report = read_route_diagnostic(args.log, args.expect, cases, args.diagnostic_prefix)
        else:
            report = (read_baseline_red(args.log, args.baseline_red_case) if args.baseline_red_case else
                      read_results(args.log, args.expect, args.cases, cases))
        if args.screenshots:
            if args.scale is None:
                parser.error('--screenshots requires --scale')
            report['screenshots'] = check_pngs(args.screenshots, args.scale)
            report['passed'] &= report['screenshots']['passed']
    except (OSError, ValueError, TypeError, KeyError) as error:
        report = {'passed': False, 'errors': [str(error)], 'tests': []}
        if args.diagnostic_prefix:
            report = diagnostic_report(report, args.diagnostic_prefix)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    outcome = 'diagnostic_passed' if args.diagnostic_prefix else 'passed'
    print(json.dumps({outcome: report[outcome], 'report': str(args.output)}))
    return 0 if report[outcome] else 1


if __name__ == '__main__':
    raise SystemExit(main())

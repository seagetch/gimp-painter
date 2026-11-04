#!/usr/bin/env python3
"""Exact sanitizer-only21 single-case invocations and fail-closed result union.

Each uses the unchanged single-plan common/Brush checks and540s ceiling.
Normal full21 at scales1/2 remains separate cross-case integration coverage.
Raw command/environment stays private; only safe checker fields export.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import check_brush_ci as gate


def read_json(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Missing/symlink sanitizer input: ' + path.name)
    return json.loads(path.read_text())


def cases(bundle):
    full = read_json(bundle / 'brush-cases.json')
    ids = full['case_ids']
    if (full['schema_version'] != 1 or full['test'] != gate.TEST or
            len(ids) != 21 or len(set(ids)) != 21 or
            any(not re.fullmatch(r'/brush-thumbnail-native-popup/[a-z0-9-]+', case) for case in ids)):
        raise ValueError('Full Brush manifest must contain21 unique leaves')
    result = []
    for index, case in enumerate(ids, 1):
        label = f'brush-sanitizer-case{index:02d}'
        relative = f'sanitizer-case-manifests/{index:02d}.json'
        manifest = read_json(bundle / relative)
        if manifest != {'schema_version': 1, 'test': gate.TEST, 'case_ids': [case]}:
            raise ValueError('Single-case manifest differs from full ordered union: ' + label)
        result.append({'label': label, 'case_manifest': relative, 'case_id': case})
    actual = {path.name for path in (bundle / 'sanitizer-case-manifests').iterdir()}
    if actual != {f'{index:02d}.json' for index in range(1, 22)}:
        raise ValueError('Extra/missing single-case sanitizer manifest')
    return result


def aggregate(bundle, logs, reports, checker):
    expected = cases(bundle)
    result = {'passed': True, 'errors': [], 'tests': [], 'cases': [],
              'scope': 'Partitioned sanitizer:21 fresh processes/profiles, one exact case each; not a single full21 process. Normal full21 at scales1/2 remains required.',
              'required_case_count': 21, 'executed_case_ids': [],
              'timeout_seconds_per_case': 540, 'total_observed_duration_seconds': 0.0,
              'raw_environment_exported': False}
    common = gate.load_common(checker)
    for case in expected:
        label, case_id = case['label'], case['case_id']
        item = {'label': label, 'required_case_id': case_id, 'passed': False}
        try:
            log = logs / (label + '.json')
            if log.is_symlink() or not log.is_file():
                raise ValueError('Missing/symlink Meson single-case log')
            rows = [json.loads(line) for line in log.read_text().splitlines() if line.strip()]
            # Exactly the same single-plan checker used for existing UI cases.
            report = common.read_results(log, [gate.TEST], case_ids=[case_id])
            gate.reject_sanitizer_diagnostics(report)
            if len(rows) != 1 or not isinstance(rows[0], dict):
                raise ValueError('Single-case invocation requires one registration')
            row = rows[0]
            command = row.get('command', [])
            if not isinstance(command, list) or not all(isinstance(x, str) for x in command):
                raise ValueError('Malformed private Meson command')
            selectors = [command[i + 1] for i, arg in enumerate(command[:-1]) if arg == '-p']
            forbidden = ('-s', '-x', '--skip-prefix', '-r', '--run-prefix', '-l', '--list', '-m')
            if (selectors != [case_id] or command.count('-p') != 1 or
                    not any(Path(arg).name == gate.TEST for arg in command) or
                    any(x.split('=')[0] in forbidden or x.startswith(('--GTestSkipCount', '--GTestSubprocess')) for x in command) or
                    any(x.startswith('-p') and x != '-p' for x in command)):
                report['errors'].append('Invocation must select only its exact single Brush leaf')
            output = str(row.get('stdout', '')) + '\n' + str(row.get('stderr', ''))
            # Reject extra/malformed TAP result lines even if they lack a path.
            tap_results = re.findall(r'^(?:not )?ok\b[^\n]*', output, re.M)
            if tap_results != ['ok 1 ' + case_id]:
                report['errors'].append('Expected one exact native TAP result, without extras or skips')
            duration = row.get('duration')
            if type(duration) not in (int, float) or not math.isfinite(duration) or duration < 0:
                report['errors'].append('Missing/invalid native invocation duration')
            else:
                item['duration_seconds'] = duration
                result['total_observed_duration_seconds'] += duration
            report['passed'] = not report['errors']
            saved_path = reports / (label + '-results.json')
            saved = read_json(saved_path)
            # Recheck raw logs; never trust an exported passed flag by itself.
            if saved.get('passed') is not True or saved.get('errors') != [] or saved.get('tests') != report['tests']:
                report['errors'].append('Saved single-case gate missing, failed or differs from raw result')
                report['passed'] = False
            item.update(passed=report['passed'], errors=report['errors'],
                        result_sha256=hashlib.sha256(saved_path.read_bytes()).hexdigest())
            result['tests'].extend(report['tests'])
            result['executed_case_ids'].extend(re.findall(r'^ok\s+\d+\s+(/\S+)\s*$', output, re.M))
        except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
            item['errors'] = [str(error)]
        if not item['passed']:
            result['passed'] = False
            result['errors'].append('Failed or incomplete sanitizer case: ' + label)
        result['cases'].append(item)
    expected_logs = {case['label'] + '.json' for case in expected}
    expected_reports = {case['label'] + '-results.json' for case in expected}
    if ({path.name for path in logs.glob('brush-sanitizer-case*.json')} != expected_logs or
            {path.name for path in reports.glob('brush-sanitizer-case*-results.json')} != expected_reports):
        result['passed'] = False
        result['errors'].append('Missing/extra sanitizer invocation evidence routes')
    required = [case['case_id'] for case in expected]
    if result['executed_case_ids'] != required or len(set(result['executed_case_ids'])) != 21:
        result['passed'] = False
        result['errors'].append('Sanitizer aggregate lacks exact21 unique ordered cases')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--plan', action='store_true')
    parser.add_argument('--logs', type=Path)
    parser.add_argument('--reports', type=Path)
    parser.add_argument('--common-checker', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        if args.plan:
            if any((args.logs, args.reports, args.common_checker, args.output)):
                raise ValueError('Planning cannot also claim a runtime result')
            for case in cases(args.bundle):
                print(case['label'] + '\t' + case['case_manifest'] + '\t' + case['case_id'])
            return 0
        if not all((args.logs, args.reports, args.common_checker, args.output)):
            raise ValueError('Aggregation requires logs, reports, unchanged checker and output')
        report = aggregate(args.bundle, args.logs, args.reports, args.common_checker)
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        report = {'passed': False, 'errors': [str(error)], 'tests': [], 'raw_environment_exported': False}
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'errors': report['errors']}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

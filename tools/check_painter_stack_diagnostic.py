#!/usr/bin/env python3
"""Export bounded historical GDB evidence, never an acceptance test result."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re

SELECTED_CASE = '/common-layer-native-popup/ordinary-tile/staged-child-1'
SOURCE_COMMIT = '978461dde961ee6f11067b8f9c68085bc442a600'
MAX_OUTPUT = 1024 * 1024


def safe_output(value):
    text = str(value or '')
    truncated = len(text) > MAX_OUTPUT
    # Prefer the tail where GDB prints its stop and backtraces. Do not export
    # Meson's inherited environment or command line fields at all.
    text = text[-MAX_OUTPUT:]
    lines = [line for line in text.splitlines()
             if not line.lstrip().startswith(('>>>', 'Inherited environment:'))]
    return '\n'.join(lines) + '\n', truncated


def read_diagnostic(path, selected_case):
    report = dict(diagnostic_only=True, acceptance_eligible=False,
                  source_commit=SOURCE_COMMIT, selected_case=selected_case, scale=2,
                  scope='Historical isolated GDB reproducer; no acceptance or pass-count use',
                  diagnostic_status='incomplete_evidence', stack_captured=False,
                  errors=[], tests=[])
    try:
        if selected_case != SELECTED_CASE:
            raise ValueError('Only the reviewed historical staged-child-1 selector is allowed')
        rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
        if len(rows) != 1 or not isinstance(rows[0], dict):
            raise ValueError('Expected exactly one historical diagnostic registration')
        row = rows[0]
        command = row.get('command', [])
        if (row.get('name', '').rsplit(' / ', 1)[-1] != 'painter-common-layer-popup' or
                not isinstance(command, list) or command[-2:] != ['-p', selected_case] or
                not any(str(arg).endswith('/gdb-stack-wrapper.sh') for arg in command)):
            raise ValueError('Wrong registration, selected case, or GDB wrapper')
        safe = {key: row.get(key) for key in ('name', 'result', 'returncode', 'duration')}
        for stream in ('stdout', 'stderr'):
            safe[stream], safe[stream + '_truncated'] = safe_output(row.get(stream))
        report['tests'] = [safe]
        output = safe['stdout'] + '\n' + safe['stderr']
        signal = bool(re.search(r'(?:Program|Thread [^\n]+) received signal SIGSEGV\b', output))
        frames = re.findall(r'^#\d+\s+[^\n]+', output, re.M)
        report.update(sigsegv_observed=signal, frame_count=len(frames))
        if row.get('result') == 'TIMEOUT':
            report['diagnostic_status'] = 'incomplete_timeout'
        elif (signal and frames and row.get('result') == 'FAIL' and
              type(row.get('returncode')) is int and row['returncode'] != 0):
            report['stack_captured'] = True
            report['diagnostic_status'] = 'sigsegv_stack_captured'
        elif re.search(r'(?:ptrace:|Operation not permitted|Could not trace|not found)', output, re.I):
            report['diagnostic_status'] = 'incomplete_debugger_or_runtime'
        else:
            report['diagnostic_status'] = 'no_sigsegv_stack_reproduced'
    except (OSError, ValueError, TypeError, KeyError) as error:
        report['errors'].append(str(error))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--selected-case', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = read_diagnostic(args.log, args.selected_case)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in
                     ('diagnostic_only', 'acceptance_eligible', 'diagnostic_status', 'stack_captured')}))
    # A successful diagnostic job means a genuine SIGSEGV stack was captured,
    # never that the historical case or current candidate passed.
    return 0 if report['stack_captured'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

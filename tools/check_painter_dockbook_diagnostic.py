#!/usr/bin/env python3
"""Export bounded pinned dockbook-warning evidence, never acceptance results."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
from check_painter_stack_diagnostic import safe_output

SOURCE_COMMIT = '3d6f62235c6bc9e2505d2c2b6ad1d42e15d081af'
SOURCE_TREE = 'a5a14a81a10857834301888a0cd2b3cf902bf17d'
SELECTED_PREFIX = '/common-layer-native-popup/clone-tree'
WARNING = ('Drawing a gadget with negative dimensions. Did you forget to allocate a size? '
           '(node tab owner GimpDockbook)')


def read_diagnostic(path, selected_prefix):
    report = dict(diagnostic_only=True, acceptance_eligible=False,
                  source_commit=SOURCE_COMMIT, source_tree=SOURCE_TREE,
                  source_identity_record='dockbook-overlay.json',
                  selected_prefix=selected_prefix, scale=1,
                  scope='Pinned production with reviewed passive test observer; no acceptance or pass-count use',
                  diagnostic_status='incomplete_evidence', stack_captured=False,
                  errors=[], tests=[])
    try:
        if selected_prefix != SELECTED_PREFIX:
            raise ValueError('Only the reviewed Clone-tree prefix is allowed')
        rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
        if len(rows) != 1 or not isinstance(rows[0], dict):
            raise ValueError('Expected exactly one dockbook diagnostic registration')
        row = rows[0]
        command = row.get('command', [])
        if (row.get('name', '').rsplit(' / ', 1)[-1] != 'painter-common-layer-popup' or
                not isinstance(command, list) or command[-2:] != ['--run-prefix', selected_prefix] or
                not any(str(arg).endswith('/gdb-dockbook-wrapper.sh') for arg in command)):
            raise ValueError('Wrong registration, selected prefix, or dockbook GDB wrapper')
        safe = {key: row.get(key) for key in ('name', 'result', 'returncode', 'duration')}
        for stream in ('stdout', 'stderr'):
            safe[stream], safe[stream + '_truncated'] = safe_output(row.get(stream))
        report['tests'] = [safe]
        output = safe['stdout'] + '\n' + safe['stderr']
        signal = bool(re.search(r'(?:Program|Thread [^\n]+) received signal SIGTRAP\b', output))
        warning = any('Gtk-WARNING' in line and WARNING in line for line in output.splitlines())
        frames = re.findall(r'^#\d+\s+[^\n]+', output, re.M)
        report.update(sigtrap_observed=signal, dockbook_warning_observed=warning, frame_count=len(frames))
        if row.get('result') == 'TIMEOUT':
            report['diagnostic_status'] = 'incomplete_timeout'
        elif (signal and warning and frames and row.get('result') == 'FAIL' and
              type(row.get('returncode')) is int and row['returncode'] != 0):
            report['stack_captured'] = True
            report['diagnostic_status'] = 'dockbook_warning_sigtrap_stack_captured'
        elif re.search(r'(?:ptrace:|Operation not permitted|Could not trace|not found)', output, re.I):
            report['diagnostic_status'] = 'incomplete_debugger_or_runtime'
        else:
            report['diagnostic_status'] = 'no_matching_dockbook_warning_stack_reproduced'
    except (OSError, ValueError, TypeError, KeyError) as error:
        report['errors'].append(str(error))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--selected-prefix', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = read_diagnostic(args.log, args.selected_prefix)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in
                     ('diagnostic_only', 'acceptance_eligible', 'diagnostic_status', 'stack_captured')}))
    return 0 if report['stack_captured'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

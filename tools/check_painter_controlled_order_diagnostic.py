#!/usr/bin/env python3
"""Attribute a bounded diagnostic stop to explicit pending-offscreen dispatch.

This active scheduling probe never supplies candidate acceptance. A normal
return can mean no update was pending and does not establish resilience.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
from check_painter_dockbook_diagnostic import read_diagnostic, SELECTED_PREFIX

MAX_TRACE = 2 * 1024 * 1024
MARKER = 'ACTIVE-CONTROLLED-OFFSCREEN-UPDATES '
BEGIN = MARKER + 'BEGIN'
RETURNED = MARKER + 'RETURNED'
# Pinned GTK3 GdkWindowType: ROOT=0, TOPLEVEL, CHILD, TEMP, FOREIGN, OFFSCREEN=5.
OFFSCREEN_TYPE = 5
MAIN_TRACE = '=== DOCKBOOK MAIN THREAD BACKTRACE (MAX 200 FRAMES) ==='
TEST_SHA256 = '78079b8209f0bb4a06f9c44ef15a2ee393c3d917da7ecad213cee8b259c61152'
CALL_LINE = 291
# Pre-run, reviewed attribution decision: locked libgdk SHA256
# cb75fbb8fbd28c507322a2d318e6a7199c4b7a4e9c60cb984bd3da613a997502
# restores its public process_updates frame then tail-jumps at0x51656 to0x51450.
# Therefore require our named test caller at the exact pinned dispatch line,
# not an optimized-away public GDK wrapper or arbitrary helper presence.


def controlled_trace(path):
    with path.open('rb') as stream:
        data = stream.read(MAX_TRACE + 1)
    if len(data) > MAX_TRACE:
        raise ValueError('Controlled trace exceeds the reviewed 2MiB bound')
    text = data.decode('utf-8')
    if 'TRACE LIMIT REACHED' in text:
        raise ValueError('Capped trace cannot establish an unreturned controlled dispatch')
    phase = None
    pending = None
    begins = returned = 0
    for number, line in enumerate(text.splitlines(), 1):
        if line.startswith('phase='):
            # Same-phase lifecycle observations can occur synchronously inside
            # dispatch; only an actual phase identity change escapes its bracket.
            identity = re.match(r'^(phase=\d+ [^ ]+ test=[^ ]+)(?: |$)', line)
            next_phase = identity[1] if identity else line
            if pending is not None and next_phase != pending['phase']:
                raise ValueError('Fixture advanced beyond an unreturned controlled dispatch')
            phase = next_phase
        if line.startswith(BEGIN):
            if pending is not None:
                raise ValueError('Nested or duplicate controlled BEGIN')
            if not (phase and re.fullmatch(
                    r'phase=\d+ fixture/controlled-offscreen-updates test=' +
                    re.escape(SELECTED_PREFIX) + r'/[a-z0-9-]+', phase)):
                raise ValueError('Controlled BEGIN lacks its selected fixture phase')
            window = re.search(r'\boffscreen=(0x[0-9a-f]+) type=(\d+)\b', line)
            if (not re.match(re.escape(BEGIN) + r' book=0x[0-9a-f]+:GimpDockbook\b', line) or
                    not window or int(window[2]) != OFFSCREEN_TYPE):
                raise ValueError('Controlled BEGIN lacks the exact offscreen dockbook target')
            pending = dict(line=number, window=window[1], phase=phase)
            begins += 1
        elif line.startswith(RETURNED):
            match = re.fullmatch(re.escape(RETURNED) + r' window=(0x[0-9a-f]+) destroyed=[01]', line)
            if not match or pending is None or match[1] != pending['window']:
                raise ValueError('Malformed or unmatched controlled RETURNED')
            pending = None
            returned += 1
        elif line.startswith(MARKER):
            raise ValueError('Unknown controlled dispatch marker')
    return dict(sha256=hashlib.sha256(data).hexdigest(), bytes=len(data), cap_reached=False,
                begin_count=begins, returned_count=returned, unreturned_dispatch=pending)


def read_controlled(log, selected_prefix, trace_path):
    report = read_diagnostic(log, selected_prefix)
    warning_stack = report['stack_captured']
    base_status = report['diagnostic_status']
    report.update(diagnostic_mode='controlled-order',
                  source_identity_record='dockbook-controlled-order-overlay.json',
                  scope='Explicit pending-offscreen dispatch on pinned production; never acceptance or pass-count use',
                  warning_stack_observed=warning_stack, stack_captured=False,
                  instrumented_test_sha256=TEST_SHA256, controlled_call_line=CALL_LINE,
                  controlled_order_reproduced=False, dispatch_frame_observed=False,
                  diagnostic_status='incomplete_controlled_trace')
    try:
        trace = controlled_trace(trace_path)
        report['controlled_trace'] = trace
    except (OSError, ValueError, UnicodeError) as error:
        report['errors'].append(str(error))
        return report
    output = '\n'.join(str(row.get(key, '')) for row in report['tests'] for key in ('stdout', 'stderr'))
    main_trace = output.split(MAIN_TRACE, 1)[1] if MAIN_TRACE in output else ''
    dispatch = False
    for block in re.split(r'(?m)(?=^#\d+\s)', main_trace):
        lines = block.splitlines()
        if not lines or not re.match(r'^#\d+\s', lines[0]):
            continue
        # GDB may wrap the source location onto the next indented line.
        frame = lines[0]
        if len(lines) > 1 and lines[1].lstrip().startswith('at '):
            frame += ' ' + lines[1].strip()
        if (re.search(r'\bcontrolled_dock_updates\s*\(', frame) and
                re.search(r'\bat (?:[^\s]+/)?app/tests/test-common-layer-popup\.cpp:' +
                          str(CALL_LINE) + r'(?:\s|$)', frame)):
            dispatch = True
    report['dispatch_frame_observed'] = dispatch
    report['public_gdk_frame_observed'] = bool(re.search(
        r'^#\d+\s+[^\n]*\bgdk_window_process_updates\s*\(', main_trace, re.M))
    pending = trace['unreturned_dispatch'] is not None
    if warning_stack and pending and dispatch:
        report.update(stack_captured=True, controlled_order_reproduced=True,
                      diagnostic_status='controlled_offscreen_warning_sigtrap_stack_captured')
    elif warning_stack and not pending:
        report['diagnostic_status'] = 'natural_warning_outside_controlled_dispatch'
    elif warning_stack and pending:
        report['diagnostic_status'] = 'incomplete_controlled_dispatch_stack'
    elif base_status.startswith('incomplete_'):
        report['diagnostic_status'] = base_status
    else:
        report['diagnostic_status'] = 'no_controlled_warning_reproduced'
    report['qualification'] = ('Only an uncapped unreturned BEGIN plus exact warning/SIGTRAP and the named test caller at its pinned dispatch line '
                               'establishes this controlled reproduction. No warning or a returned dispatch may '
                               'mean no update was pending; neither is acceptance or resilience proof.')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--selected-prefix', required=True)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = read_controlled(args.log, args.selected_prefix, args.trace)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('diagnostic_only', 'acceptance_eligible',
          'diagnostic_mode', 'diagnostic_status', 'controlled_order_reproduced')}))
    return 0 if report['controlled_order_reproduced'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

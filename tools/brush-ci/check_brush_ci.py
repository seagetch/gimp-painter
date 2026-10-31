#!/usr/bin/env python3
"""Draft Brush-only gate. Does not replace or weaken the task-307 checker.

Only allowlisted Meson output is exported; raw command/environment stays private.
Positive checks delegate to the unchanged common checker and additionally reject
selected-output sanitizer diagnostics, including child reports. Baseline and images
have separate Brush contracts. PNG checks do not establish visual acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

CASE = '/brush-thumbnail-native-popup/actual-entry'
LOWER_CASE = '/brush-thumbnail-native-popup/lower-controls-actual-input'
TEST = 'painter-compact-options'
GEOMETRY = 'painter-brush-geometry'
GEOMETRY_CASES = ['/painter/geometry/old-masks', '/painter/geometry/options', '/painter/geometry/defaults', '/painter/geometry/provenance', '/painter/geometry/cache', '/painter/geometry/invalid', '/painter/geometry/default-dynamics']
CHOICE = 'choice-lifecycle'
CHOICE_CASES = ['/choice-lifecycle/direct-owned-qdata', '/choice-lifecycle/param-spec-ownership-and-values', '/choice-lifecycle/wire-roundtrip', '/choice-lifecycle/wire-truncated-choice']
PROTOCOL = 'plug-in-protocol'
PROTOCOL_CASES = ['/plug-in-protocol/query', '/plug-in-protocol/init', '/plug-in-protocol/host-quit', '/plug-in-protocol/explicit-quit', '/plug-in-protocol/success-open', '/plug-in-protocol/success-close', '/plug-in-protocol/error-open', '/plug-in-protocol/error-close', '/plug-in-protocol/failed-main-write', '/plug-in-protocol/failed-main-write-recoverable', '/plug-in-protocol/temporary-quit', '/plug-in-protocol/temporary-main']
SKIP = re.compile(r'^\s*(?:(?:not )?ok\b[^\n]*|\d+\.\.\d+\s*)#\s*(?:SKIP|TODO)\b', re.I | re.M)
BODY_ERROR = re.compile(r"test-painter-brush-thumbnail-popup\.inc:\d+:brush_popup_assert_body:\s*['\"]body['\"] should not be NULL")
STOCK = 'Actual Brush thumbnail carrier=GimpContainerPopup extended-body=0 selector=0 preview=0'
BAD = re.compile(r'AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:|Segmentation fault|SIGSEGV|Traceback|CRITICAL|WARNING', re.I)
# Child plug-ins may emit a sanitizer report without making the parent Meson
# registration fail. Reject actual diagnostics in selected test output, not
# benign mentions of instrumentation or build flags. No sanitizer is suppressed.
SANITIZER_DIAGNOSTIC = re.compile(
    r"\b(?:ERROR|WARNING|SUMMARY):\s*(?:AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer)\b"
    r"|\b(?:AddressSanitizer|UndefinedBehaviorSanitizer):\s*(?:DEADLYSIGNAL|CHECK failed|nested bug)\b"
    r"|\bLeakSanitizer has encountered a fatal error\b"
    r"|\bruntime error:", re.I)


def reject_sanitizer_diagnostics(report):
    """Tighten only this Brush wrapper; keep parent/TAP results unchanged."""
    for row in report.get('tests', []):
        streams = [key for key in ('stdout', 'stderr')
                   if SANITIZER_DIAGNOSTIC.search(str(row.get(key, '')))]
        if streams:
            report['passed'] = False
            report['errors'].append('Sanitizer diagnostic in selected registration '
                                    + str(row.get('name', '')) + ' ('
                                    + ', '.join(streams) + '), even if parent reports OK')
    return report


def baseline_red(rows):
    errors = []
    if len(rows) != 1 or not isinstance(rows[0], dict):
        return {'passed': False, 'errors': ['Expected exactly one Meson registration'], 'tests': []}
    row = rows[0]
    command = row.get('command', [])
    if not isinstance(command, list) or not all(isinstance(x, str) for x in command):
        command = []
    selected = [command[i + 1] for i, arg in enumerate(command[:-1]) if arg == '-p']
    if selected != [CASE] or any(x.split('=')[0] in ('-s', '-x', '--skip-prefix', '-r', '--run-prefix') for x in command):
        errors.append('Must select only the exact Brush actual-entry case, without skip controls')
    if not any(Path(arg).name == TEST for arg in command):
        errors.append('Command is not the compact-options executable')
    if row.get('name', '').rsplit(' / ', 1)[-1] != TEST:
        errors.append('Wrong Meson registration')
    # Native GLib assertions abort; do not accept arbitrary build/startup errors.
    if row.get('result') != 'FAIL' or type(row.get('returncode')) is not int or row['returncode'] not in (-6, 134):
        errors.append('Expected FAIL with native assertion abort, not pass/skip/timeout/other failure')
    output = str(row.get('stdout', '')) + '\n' + str(row.get('stderr', ''))
    match = BODY_ERROR.search(output)
    if STOCK not in output or not match or output.index(STOCK) > match.start():
        errors.append('Missing stock-selector/missing-extended-body oracle')
    if SKIP.search(output) or BAD.search(output):
        errors.append('Unexpected skip, warning, crash or instrumentation error')
    if re.search(r'^ok\s+\d+\s+/', output, re.M):
        errors.append('Baseline selected case must not report a pass')
    # Reject unrelated assertions even if a copied expected error is present.
    assertion_lines = [line for line in output.splitlines() if re.search(r'(?:ERROR:|Bail out!).*(?:assertion|should)', line)]
    if not assertion_lines or any(not BODY_ERROR.search(line) for line in assertion_lines):
        errors.append('Unrelated assertion or missing assertion line')
    return {'passed': not errors, 'errors': errors, 'selected_case': CASE,
            'scope': 'Expected stock Brush missing-extended-body RED; not passing baseline UI',
            'tests': [{k: row.get(k) for k in ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}]}


def screenshot(path: Path, scale: int, output: str, selected_case: str = CASE):
    errors = []
    image = {'file': path.name, 'scale': scale}
    if path.is_symlink() or not path.is_file():
        errors.append('Screenshot must be an existing regular, non-symlink file')
    else:
        data = path.read_bytes()
        if len(data) < 24 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
            errors.append('Invalid PNG header')
        else:
            width, height = struct.unpack('>II', data[16:24])
            image.update(width=width, height=height, size_bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
            if (width, height) != (1080, 1920):
                errors.append('Wrong physical dimensions; require 1080x1920 at either scale')
    geometry = re.findall(r'Brush popup logical=(-?\d+)/(-?\d+) (\d+)x(\d+) scale=(\d+)', output)
    if len(geometry) != 1 or int(geometry[0][-1]) != scale:
        errors.append('Missing/ambiguous Brush geometry marker or wrong actual widget scale')
    elif any(int(x) < 0 for x in geometry[0][:2]) or any(int(x) <= 0 for x in geometry[0][2:4]):
        errors.append('Invalid Brush geometry')
    if selected_case == LOWER_CASE:
        markers = re.findall(r'Brush lower controls reachable=5 numeric-entry=41 preview-visible=1 wheel-events=(\d+) scale=(\d+)', output)
        fields = re.findall(r'Brush lower physical control (radius|spikes|hardness|aspect-ratio|angle) focused/edited value=', output)
        if len(markers) != 1 or int(markers[0][1]) != scale:
            errors.append('Missing/ambiguous lower-control actual-input and bottom-preview marker')
        if len(fields) != 5 or set(fields) != {'radius', 'spikes', 'hardness', 'aspect-ratio', 'angle'}:
            errors.append('Missing/duplicate lower-control physical edit evidence')
        image['capture_case'] = LOWER_CASE
        image['physical_outer_scroll_events'] = int(markers[0][0]) if len(markers) == 1 else None
    elif selected_case != CASE:
        errors.append('Unsupported capture case')
    return {'passed': not errors, 'errors': errors, 'image': image,
            'visual_inspection': 'PENDING: decode/view real pixels; inspect controls, preview, grid and lower scroller'}


def load_common(path):
    spec = importlib.util.spec_from_file_location('unchanged_common_ui_gate', path)
    if spec is None or spec.loader is None:
        raise ValueError('Cannot load unchanged positive checker')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--log', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--baseline-red', action='store_true')
    p.add_argument('--common-checker', type=Path)
    p.add_argument('--case-manifest', type=Path)
    p.add_argument('--screenshot', type=Path)
    p.add_argument('--scale', type=int, choices=[1, 2])
    a = p.parse_args()
    try:
        rows = [json.loads(x) for x in a.log.read_text().splitlines() if x.strip()]
        if a.baseline_red:
            if a.screenshot or a.case_manifest:
                raise ValueError('Baseline RED cannot claim screenshot/positive acceptance')
            report = baseline_red(rows)
        else:
            if not a.common_checker or not a.case_manifest:
                raise ValueError('Positive gate requires unchanged checker and exact case manifest')
            manifest = json.loads(a.case_manifest.read_text())
            ids = manifest['case_ids']
            registration = manifest['test']
            if manifest['schema_version'] != 1 or registration not in (TEST, 'painter-toolbar-window', PROTOCOL, CHOICE, GEOMETRY, 'color-config-ownership', 'display-renderer-ownership', 'canvas-ownership', 'gimp-frame-style-idempotence') or not ids or len(ids) != len(set(ids)):
                raise ValueError('Malformed positive manifest')
            if registration == PROTOCOL and ids != PROTOCOL_CASES:
                raise ValueError('Protocol gate requires all exact twelve lifecycle cases')
            if registration == CHOICE and ids != CHOICE_CASES:
                raise ValueError('Choice gate requires all four exact ownership and wire lifecycle cases')
            if registration == GEOMETRY and ids != GEOMETRY_CASES:
                raise ValueError('Geometry gate requires all exact seven model cases')
            ownership_cases = {
                'canvas-ownership': ['/ownership/canvas/unrealized-padding', '/ownership/canvas/realized-padding'],
                'gimp-frame-style-idempotence': ['/gimp-frame-style/unchanged', '/gimp-frame-style/different-weight', '/gimp-frame-style/same-weight-extra-attribute', '/gimp-frame-style/partial-range-bold', '/gimp-frame-style/equal-list-different-instance', '/gimp-frame-style/replacement-label', '/gimp-frame-style/style-change'],
                'color-config-ownership': ['/ownership/config/initial', '/ownership/config/replacement'],
                'display-renderer-ownership': ['/ownership/display/parity', '/ownership/renderer/public-smoke'],
            }
            if registration in ownership_cases and ids != ownership_cases[registration]:
                raise ValueError('Supplementary gate requires all exact registered cases')
            diagnostic_ids = {
                '/brush-exit-diagnostic/harness-only',
                '/brush-exit-diagnostic/fixture-no-popup',
                '/brush-thumbnail-native-popup/private-target-release',
            }
            diagnostic = manifest.get('diagnostic_only', False)
            if diagnostic is not False and diagnostic is not True:
                raise ValueError('Diagnostic marker must be Boolean')
            if diagnostic and (registration != TEST or len(ids) != 1 or ids[0] not in diagnostic_ids or a.screenshot):
                raise ValueError('Only exact single-case causality diagnostics are allowed')
            if any(x.startswith('/brush-exit-diagnostic/') for x in ids) and not diagnostic:
                raise ValueError('Exit diagnostic cannot be an acceptance manifest')
            report = load_common(a.common_checker).read_results(a.log, [registration], case_ids=ids)
            reject_sanitizer_diagnostics(report)
            if registration == 'gimp-frame-style-idempotence':
                for row in report.get('tests', []):
                    output = '\n'.join(str(row.get(key, '')) for key in ('stdout', 'stderr'))
                    if re.search(r'\b(?:CRITICAL|WARNING|ERROR)\b|Traceback|Bail out!|^not ok\b', output, re.M):
                        report['passed'] = False
                        report['errors'].append('Runtime diagnostic in frame-style regression')
            if diagnostic:
                report['diagnostic_only'] = True
                report['acceptance_eligible'] = False
            if a.screenshot:
                if a.scale is None or registration != TEST or ids not in ([CASE], [LOWER_CASE]):
                    raise ValueError('Screenshot must come from one exact actual-entry or lower-control capture invocation')
                output = '\n'.join(str(row.get(key, '')) for row in rows for key in ('stdout', 'stderr'))
                report['screenshot'] = screenshot(a.screenshot, a.scale, output, ids[0])
                report['passed'] &= report['screenshot']['passed']
        report['raw_environment_exported'] = False
    except (OSError, ValueError, TypeError, KeyError) as error:
        report = {'passed': False, 'errors': [str(error)], 'tests': []}
    a.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'report': str(a.output)}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

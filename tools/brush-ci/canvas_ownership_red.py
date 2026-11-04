#!/usr/bin/env python3
"""Bounded old-canvas ownership RED evidence; never candidate acceptance."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex

OLD_SHA = 'c4a10375777bae95a9f8a9dd8014935045cd8f5c91791ddd8fac77cbd10bb33e'
TEST_SHA = '2621671a93a054a1f4e1f3f7b5f922d1c4f79226f6ca8d7d2ea09e21f9455c7f'
FIXTURE = 'app/tests/color-ownership/fixtures/gimpcanvas-c9.c'
TEST = 'app/tests/color-ownership/test-canvas.c'
TARGET = 'canvas-ownership-old-provider'
CASES = ('/ownership/canvas/unrealized-padding', '/ownership/canvas/realized-padding')
APIS = ('gimp_canvas_get_type', 'gimp_canvas_new', 'gimp_canvas_get_layout', 'gimp_canvas_set_padding')
ASSERTION = re.compile(r'^(?:Bail out! )?ERROR:[^\n]*\btest-canvas\.c:\d+:padding_lifetime: assertion failed \(last_gone == 1\): \(0 == 1\)$')

def intended_assertion(line, case):
    prefix = re.fullmatch(r'not ok (?:1 )?' + re.escape(case) + r' - (ERROR:.*)', line)
    return bool(ASSERTION.fullmatch(prefix.group(1) if prefix else line))

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def base(kind):
    return {'format': 1, 'kind': kind, 'control_passed': False,
            'negative_control': True, 'acceptance_eligible': False, 'errors': [],
            'raw_environment_exported': False}

def rows_from(path):
    text = Path(path).read_text()
    # Real meson-logs/*.json is JSON Lines, one registration per line.
    return [json.loads(line) for line in text.splitlines() if line.strip()]

def check_case(rows, case):
    r = base('canvas-old-provider-case'); e = r['errors']; r['selected_case'] = case
    if case not in CASES:
        e.append('Unknown exact leaf'); return r
    if len(rows) != 1 or not isinstance(rows[0], dict):
        e.append('Exactly one Meson registration required'); return r
    t = rows[0]
    r['test'] = {k: t.get(k) for k in ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}
    if str(t.get('name', '')).split(' / ')[-1] != TARGET: e.append('Wrong Meson target')
    if t.get('result') != 'EXPECTEDFAIL' or type(t.get('returncode')) is not int or t['returncode'] != -6:
        e.append('Expected Meson EXPECTEDFAIL with real child SIGABRT (-6), not generic failure/timeout/skip/success')
    cmd = t.get('command')
    if not isinstance(cmd, list) or not cmd or not all(isinstance(x, str) for x in cmd):
        cmd = []; e.append('Missing actual Meson argv')
    if cmd:
        if Path(cmd[0]).name != TARGET: e.append('Wrong executed provider')
        # No alternate selection, wrappers, skip flags, or trailing extra selectors.
        if cmd[1:] != ['-p', case]: e.append('Expected only exact -p leaf selection')
    stdout, stderr = t.get('stdout'), t.get('stderr')
    if not isinstance(stdout, str) or not isinstance(stderr, str):
        e.append('Raw stdout and stderr required'); stdout = stderr = ''
    output = stdout + '\n' + stderr
    if not re.search(r'^TAP version \d+\s*$', stdout, re.M): e.append('Missing TAP execution')
    for part in ('ownership', 'canvas'):
        if '# Start of ' + part + ' tests' not in stdout: e.append('Missing TAP hierarchy: ' + part)
    for stream in (stdout, stderr):
        lines = [line for line in stream.splitlines() if 'assertion failed' in line]
        if len(lines) != 1 or not intended_assertion(lines[0], case): e.append('Wrong/missing exact lifetime assertion')
    if re.search(r'^\s*ok\b|^\s*1\.\.0\b|#\s*(?:SKIP|TODO)\b', output, re.M | re.I):
        e.append('Completed/zero/skip TAP does not prove intended abort')
    if re.search(r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:|DEADLYSIGNAL|CRITICAL|WARNING|Traceback|timed? out|timeout', output, re.I):
        e.append('Unrelated sanitizer/runtime/timeout diagnostic')
    not_ok = re.findall(r'^not ok .*$', output, re.M)
    if len(not_ok) > 1 or any(not intended_assertion(line, case) for line in not_ok):
        e.append('Wrong or multiple failing TAP leaf')
    if not re.search(r'^Bail out!(?: ERROR:.*)?$', stdout, re.M):
        e.append('Missing TAP bailout')
    # Reject additional GLib ERROR lines, including unrelated assertions.
    for line in output.splitlines():
        if re.search(r'\bERROR:', line) and not intended_assertion(line, case): e.append('Unrelated ERROR diagnostic')
    r['control_passed'] = not e
    return r

def _resolved(path, directory):
    p = Path(path)
    return (p if p.is_absolute() else directory / p).resolve()

def check_provider(source, build, map_path):
    raw_map = Path(map_path).absolute()
    source, build, map_path = Path(source).resolve(), Path(build).resolve(), Path(map_path).resolve()
    expected_map = build / 'app/tests/color-ownership/canvas-ownership-old-provider.map'
    if raw_map != map_path or map_path != expected_map:
        raise ValueError('Map must be unsymlinked exact build target output')
    r = base('canvas-old-provider-provenance'); e = r['errors']
    fixture, test = source / FIXTURE, source / TEST
    if fixture.resolve() != fixture or test.resolve() != test: raise ValueError('Source symlinks not accepted')
    if sha(fixture) != OLD_SHA: e.append('Old fixture is not exact C9 source')
    if sha(test) != TEST_SHA: e.append('Regression test differs from reviewed candidate test')
    cc_path, opts_path = build / 'compile_commands.json', build / 'meson-info/intro-buildoptions.json'
    cc, opts = json.loads(cc_path.read_text()), json.loads(opts_path.read_text())
    values = [x.get('value') for x in opts if x.get('name') == 'b_sanitize']
    if len(values) != 1 or values[0] not in ('none', [], ['none']):
        e.append('Old provider must be normal non-sanitized build')
    objects = {}
    diagnostics = []; diagnostic_total = 0
    for key, src, target in (('old', fixture, TARGET), ('old_test', test, TARGET), ('candidate_test', test, 'canvas-ownership')):
        matching = []
        for entry in cc:
            directory = Path(entry['directory']).resolve()
            observed = _resolved(entry['file'], directory)
            # Diagnostic suffix matching never admits an input: exact resolved
            # source identity below remains mandatory. Export no compiler argv.
            if observed != src and not observed.as_posix().endswith('/' + str(src.relative_to(source))): continue
            args = entry.get('arguments')
            if args is None: args = shlex.split(entry.get('command', ''))
            if not isinstance(args, list) or not all(isinstance(x, str) for x in args): raise ValueError('Invalid compile argv')
            outputs = [args[i + 1] for i, x in enumerate(args[:-1]) if x == '-o']
            if len(outputs) != 1: raise ValueError('Ambiguous compile output')
            raw_obj = Path(outputs[0])
            raw_obj = raw_obj if raw_obj.is_absolute() else directory / raw_obj
            obj = _resolved(outputs[0], directory)
            if target + '.p' not in obj.parts: continue
            diagnostic_total += 1
            if len(diagnostics) < 6:
                diagnostics.append({'key': key, 'expected_source': str(src),
                    'observed_source': str(observed), 'output_object': str(obj),
                    'source_identity_matches': observed == src})
            if observed != src: continue
            if not obj.is_relative_to(build) or raw_obj.absolute() != obj:
                e.append('Compile output escapes build or is symlinked')
            if any(x.startswith('-fsanitize') for x in args): e.append('Sanitized compile command in old-provider proof')
            if '-c' not in args or sum(_resolved(x, directory) == src for x in args if not x.startswith('-')) != 1:
                e.append('Source is not directly compiled once')
            matching.append(obj)
        if len(matching) != 1: e.append('Expected one exact compile entry: ' + key)
        else: objects[key] = matching[0]
    r['compile_input_diagnostics'] = diagnostics
    r['compile_input_diagnostics_truncated'] = diagnostic_total > len(diagnostics)
    text = map_path.read_text()
    if 'Cross Reference Table' not in text: e.append('GNU --cref table required')
    # LOAD of the archive alone is fine. Any member occurrence means extraction
    # or competing definition: fail closed, even in discarded sections.
    if re.search(r'[^\s()]+\.a\([^\n)]*gimpcanvas\.c\.o\)', text, re.M):
        e.append('Production archive canvas object extracted')
    table = text.split('Cross Reference Table', 1)[-1]
    if 'old' in objects:
        old = objects['old']
        loads = [_resolved(x, build) for x in re.findall(r'^LOAD\s+(\S+)\s*$', text, re.M)]
        if old not in loads: e.append('Old direct object not LOADed')
        for api in APIS:
            definitions = re.findall(r'^' + re.escape(api) + r'\s+(\S+)\s*$', table, re.M)
            if len(definitions) != 1 or _resolved(definitions[0], build) != old:
                e.append('API not defined by exact direct old object: ' + api)
    binary = build / 'app/tests/color-ownership' / TARGET
    output_paths = re.findall(r'^OUTPUT\((\S+)(?:\s+[^)]*)?\)\s*$', text, re.M)
    if len(output_paths) != 1 or _resolved(output_paths[0], build) != binary:
        e.append('Map output does not identify old-provider executable')
    evidence = (cc_path, opts_path, map_path, binary, *objects.values())
    for path in evidence:
        if path.resolve() != path or not path.is_relative_to(build):
            raise ValueError('Evidence is symlinked or outside build: ' + str(path))
    r['inputs'] = {'source/' + FIXTURE: sha(fixture), 'source/' + TEST: sha(test)}
    r['inputs'].update({'build/' + str(p.relative_to(build)): sha(p) for p in evidence})
    r['objects'] = {k: str(v.relative_to(build)) for k, v in objects.items()}
    r['api_definitions'] = list(APIS)
    r['binary_path'] = str(binary)
    r['binary_sha256'] = sha(binary)
    r['control_passed'] = not e
    return r

def aggregate(paths, provider_path):
    r = base('canvas-old-provider-aggregate'); e = r['errors']; reports = []
    provider = json.loads(Path(provider_path).read_text())
    if (provider.get('kind') != 'canvas-old-provider-provenance' or
        provider.get('control_passed') is not True or provider.get('errors') != [] or
        provider.get('negative_control') is not True or provider.get('acceptance_eligible') is not False):
        e.append('Invalid provider provenance receipt')
    binary = Path(provider.get('binary_path', ''))
    if (not binary.is_absolute() or binary.name != TARGET or binary.resolve() != binary or
        sha(binary) != provider.get('binary_sha256')):
        e.append('Provider executable missing/changed/symlinked')
    for path in paths:
        j = json.loads(Path(path).read_text()); reports.append(j)
        if j.get('kind') != 'canvas-old-provider-case' or j.get('control_passed') is not True or j.get('acceptance_eligible') is not False or j.get('negative_control') is not True:
            e.append('Invalid case receipt')
        if not j.get('log_path') or not j.get('log_sha256'):
            e.append('Missing hashed raw log'); continue
        log = Path(j['log_path'])
        if sha(log) != j['log_sha256']: e.append('Raw log changed')
        raw_rows = rows_from(log)
        checked = check_case(raw_rows, j.get('selected_case'))
        if checked['control_passed']:
            executed = Path(raw_rows[0]['command'][0])
            if not executed.is_absolute() or executed.resolve() != binary or executed.absolute() != binary:
                e.append('Case command did not execute proven old-provider binary')
        if not checked['control_passed']: e.append('Raw log revalidation failed')
    if len(reports) != 2 or sorted(x.get('selected_case', '') for x in reports) != sorted(CASES):
        e.append('Both distinct exact leaves required')
    r['inputs'] = {str(Path(p).resolve()): sha(p) for p in [*paths, provider_path]}
    r['binary_sha256'] = provider.get('binary_sha256')
    r['selected_cases'] = [x.get('selected_case') for x in reports]
    r['control_passed'] = not e
    return r

def main():
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument('--log', type=Path); mode.add_argument('--provider', type=Path)
    mode.add_argument('--aggregate', type=Path, nargs=2)
    p.add_argument('--provider-receipt', type=Path); p.add_argument('--case-id'); p.add_argument('--source', type=Path); p.add_argument('--build', type=Path)
    p.add_argument('--output', type=Path, required=True); a = p.parse_args()
    try:
        if a.log:
            if not a.case_id or a.source or a.build or a.provider_receipt: raise ValueError('Case mode accepts log/case-id/output only')
            r = check_case(rows_from(a.log), a.case_id)
            r.update(log_path=str(a.log.resolve()), log_sha256=sha(a.log))
        elif a.provider:
            if not a.source or not a.build or a.case_id or a.provider_receipt: raise ValueError('Provider mode needs source/build only')
            r = check_provider(a.source, a.build, a.provider)
        else:
            if a.source or a.build or a.case_id or not a.provider_receipt:
                raise ValueError('Aggregate mode requires two reports and provider-receipt/output only')
            r = aggregate(a.aggregate, a.provider_receipt)
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as exc:
        r = base('canvas-old-provider-error'); r['errors'].append(str(exc))
    a.output.write_text(json.dumps(r, indent=2) + '\n')
    return 0 if r['control_passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())

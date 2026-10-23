#!/usr/bin/env python3
"""Validate the bounded toolbar evidence receipt; does not rerun native tests."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

def read(name):
    return json.loads((HERE / name).read_text())

def require(condition, message):
    if not condition:
        raise SystemExit('FAIL: ' + message)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def tap(text, count):
    lines = text.splitlines()
    require(f'1..{count}' in lines, 'exact TAP plan')
    require(sum(line.startswith('ok ') for line in lines) == count, 'exact TAP pass count')
    require(not any(re.search(r'(?i)#\s*(skip|todo)\b', line) or line.startswith('not ok ') for line in lines), 'no skipped/TODO/failed TAP cases')

names = {'transfer-global-actions', 'transfer-radio-notification',
         'transfer-action-publication', 'transfer-radio-command-supersession'}
red = read('expected-red.json')
red_names = {'run': 'transfer-global-actions', **{name: name for name in names if name != 'transfer-global-actions'}}
require(set(k for k,v in red.items() if isinstance(v,dict) and 'expected_failure' in v) == set(red_names), 'exact four baseline reds')
for key,name in red_names.items():
    row = red[key]
    require(any(name in line for line in row['expected_failure']), 'baseline failure names its case')
    require(any('FAIL' in line and 'SIGABRT' in line for line in row['result']), 'baseline assertion failure')
    require(any('1080x1920x24' in line and 'empty authority rejected' in line for line in row['xvfb']), 'baseline authenticated portrait')
require(len(red['production_baseline']) == 10, 'ten baseline production files')
for row in red['production_baseline'].values():
    require(row['matches_head'] and row['baseline_sha256'] == row['head_sha256'], 'unchanged baseline production')

candidate = read('candidate-results.json')
require(candidate['geometry'] == '1080x1920x24' and candidate['oncanvas_ui'] is True, 'candidate portrait/oncanvas')
require(candidate['full_timeout_seconds'] == 240 and candidate['subprocess_timeout_seconds'] == 60, 'original budgets')
rows = candidate['results']
require(len(rows) == 5, 'four focused plus full result')
focused = set()
for row in rows:
    require(row['result'] == 'OK' and row['returncode'] == 0, 'ordinary test exit')
    require('env' not in row, 'no inherited environment')
    if '-p' in row['command']:
        focused.add(row['command'][-1].rsplit('/',1)[-1]); tap(row['stdout'],1)
    else:
        tap(row['stdout'],26)
        require(row['duration'] < 240, 'ordinary timeout preserved')
require(focused == names, 'exact four focused cases')

asan = read('asan/results.json')
require(asan['whole_application_instrumented'] is False and asan['leak_detection'] is False, 'bounded sanitizer disclosure')
require(len(asan['results']) == 1, 'one full sanitizer run')
row = asan['results'][0]
require(row['exit_code'] == 0 and row['tap_ok'] == 26 and row['timeout_seconds'] == 240 and row['duration_seconds'] < 240, 'sanitizer full26 original budget')
tap((HERE/'asan/painter-toolbar-window.stdout').read_text(),26)
stderr = (HERE/'asan/painter-toolbar-window.stderr').read_text()
require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:', stderr), 'no sanitizer diagnostic')
build = read('asan/build.json')
require(len(build['sources']) == 9 and all(p.endswith('.c') for p in build['sources']), 'nine C instrumented units')
require(build['executable_sha256'] == row['executable_sha256'], 'sanitizer binary identity')
for path,sha in build['source_sha256'].items():
    require(digest(ROOT/path) == sha, 'sanitizer current source: '+path)
for path,sha in read('green-source-binary.json').items():
    if path != 'binary_sha256':
        require(digest(ROOT/path) == sha, 'ordinary current source: '+path)

headers = read('header-compile-result.json')
require(headers['status'] == 'PASS' and headers['compiled_probes'] == 407 and headers['c_probes'] == 193 and headers['cpp_probes'] == 214, 'compiled header probes')
for path in ['app/widgets/gimpaction.h','app/display/gimppaintercanvasui.h']:
    require(digest(ROOT/path) == headers['source_sha256'][path], 'compiled header source: '+path)
require('Ran 17 tests' in (HERE/'header-route-unit.log').read_text() and (HERE/'header-route-unit.log').read_text().rstrip().endswith('OK'), '17 header rejection controls')
images = sorted((HERE/'portrait').glob('*.png'))
require(len(images) == 3, 'three portrait frames')
for path in images:
    data=path.read_bytes()
    require(data[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24]) == (1080,1920), 'PNG portrait dimensions')
manifest = read('evidence-sha256.json')
actual = {p.relative_to(HERE).as_posix() for p in HERE.rglob('*') if p.is_file() and p.name != 'evidence-sha256.json' and '__pycache__' not in p.parts}
require(set(manifest) == actual, 'complete recursive artifact inventory')
for path,sha in manifest.items():
    require(digest(HERE/path) == sha, 'artifact hash: '+path)
print('PASS: red4 / focused4 / ordinary26 / bounded ASan+UBSan26; no skips; source and recursive artifact hashes; portrait1080x1920; header407+17')

#!/usr/bin/env python3
"""Verify the bounded file-save-entry receipt, not rerun native tests."""
import hashlib,json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def load(name):return json.loads((HERE/name).read_text())
def require(value,why):
    if not value:raise SystemExit('FAIL: '+why)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def tap(text):
    lines=text.splitlines()
    require('1..38' in lines and sum(l.startswith('ok ') for l in lines)==38,'full38 TAP')
    require(not any(l.startswith('not ok ') or re.search(r'(?i)#\s*(skip|todo)\b',l) for l in lines),'no skipped/failed cases')
    for name in ['roundtrip','failures','state-preconditions','handle']:
        require(any(l.startswith('ok ') and l.endswith('/file-save/'+name) for l in lines),'new case '+name)
rows=load('normal.json');require(len(rows)==3 and all(r['result']=='OK' and r['returncode']==0 and 'env' not in r for r in rows),'ordinary test outcomes')
native=next(r for r in rows if r['name'].endswith('/ gimp-painter-mybrush'));tap(native['stdout']);require(native['duration']<60,'ordinary budget')
receipt=load('ordinary-receipt.json');require(receipt['test_budget_seconds']==60 and len(receipt['linked_symbol'])==1,'original budget and exact linked symbol')
require(any('-std=c11' in c and 'test-gimp-painter-mybrush.c' in c for c in receipt['compile_commands']),'real C11 caller')
require(any('-std=c++14' in c and 'test-painter-mybrush-duplicate-handle.cpp' in c for c in receipt['compile_commands']),'real C++14 owner')
for path,h in receipt['source_sha256'].items():require(sha(ROOT/path)==h,'current tested source '+path)
run=load('asan/results.json');require(not run['leak_detection'] and not run['whole_application_instrumented'],'bounded sanitizer disclosure');require(len(run['results'])==1,'one sanitizer target');row=run['results'][0];require(row['exit_code']==0 and row['tap_ok']==38 and row['timeout_seconds']==60 and row['duration_seconds']<60,'sanitizer full38 original budget');tap((HERE/'asan/gimp-painter-mybrush.stdout').read_text());require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',(HERE/'asan/gimp-painter-mybrush.stderr').read_text()),'no sanitizer diagnostic')
build=load('asan/build.json');require(not build['changed_during_build'],'sanitizer inputs stable');require(build['executables']['gimp-painter-mybrush']['sha256']==row['executable_sha256'],'sanitizer binary identity')
for path,h in receipt['source_sha256'].items():require(build['sources_sha256'][path]==h,'same ordinary/sanitizer own source '+path)
headers=load('header-compile-result.json');require(headers['status']=='PASS' and headers['compiled_probes']==411 and headers['c_probes']==195 and headers['cpp_probes']==216,'compiled C11/C++14 closure');path='app/core/gimpmypaintbrush-save.h';require(headers['source_sha256'][path]==receipt['source_sha256'][path],'new header compiled exact source')
require(load('historical-contract.json')['canonical_row']==44,'exact historical row');require(load('preimplementation-observation.json')['returncode']==1 and load('preimplementation-observation.json')['matches']=='','preimplementation symbol observation')
canonical=next(l for l in (ROOT/'migration/contracts/cpp-c-entry-map.tsv').read_text().splitlines() if l.startswith('gimp_mypaint_brush_save\t'));require(canonical.split('\t')[-1].startswith('PARTIAL:'),'semantic limits retained in canonical row')
manifest=load('artifact-sha256.json');actual={p.relative_to(HERE).as_posix() for p in HERE.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts};require(set(manifest)==actual,'complete recursive artifact set')
for path,h in manifest.items():require(sha(HERE/path)==h,'artifact '+path)
print('PASS: file-save entry row44 bounded; normal38 + ASan/UBSan38; no skips; original60s; C11/C++14 and source/artifact identities; canonical PARTIAL retained')

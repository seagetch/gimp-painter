#!/usr/bin/env python3
"""Validate recorded bounded evidence; does not rerun the native application."""
import hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def load(n):return json.loads((HERE/n).read_text())
def require(v,w):
    if not v:raise SystemExit('FAIL: '+w)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def tap(t):
    lines=t.splitlines()
    require('1..42' in lines and sum(l.startswith('ok ') for l in lines)==42,'full42 TAP')
    require(not any(l.startswith('not ok ') or re.search(r'(?i)#\s*(skip|todo)\b',l) for l in lines),'no failed/skipped cases')
    for name in ('named-save','native-stream','encode-failure-named','encode-failure-stream'):
        require(any(l.startswith('ok ') and l.endswith('/live-icon/'+name) for l in lines),'new case '+name)
red=load('expected-red.json');require(len(red['results'])==2,'two genuine baseline failures')
baseline=subprocess.check_output(['git','show',red['baseline_commit']+':app/core/gimppaintermybrush.cpp'],cwd=ROOT)
require(hashlib.sha256(baseline).hexdigest()==red['unchanged_provider_sha256'],'unchanged baseline provider')
for row in red['results']:
    require(row['result']=='FAIL' and row['returncode']!=0 and '(52 == 164)' in row['stdout'],'actual stale-pixel assertion red')
rows=load('normal.json');require(len(rows)==1 and rows[0]['result']=='OK' and rows[0]['returncode']==0 and rows[0]['duration']<60,'ordinary outcome/budget');tap(rows[0]['stdout'])
r=load('ordinary-receipt.json');require(r['test_budget_seconds']==60,'original timeout')
for p,h in r['source_sha256'].items():require(sha(ROOT/p)==h,'tested source '+p)
require(any('-std=c11' in c and 'GIMP_TEST_MYBRUSH_WRAP_PIXBUF_ENCODE' in c for c in r['compile_commands']),'real C11 fault caller')
s=(ROOT/'app/tests/meson.build').read_text();require(r['owned_meson_block'] in s,'owned build wiring');require(hashlib.sha256(r['owned_meson_block'].encode()).hexdigest()==r['owned_meson_block_sha256'],'build block identity')
run=load('asan/results.json');require(not run['leak_detection'] and not run['whole_application_instrumented'],'bounded sanitizer disclosure');require(len(run['results'])==1,'one sanitizer suite');row=run['results'][0]
require(row['exit_code']==0 and row['tap_ok']==42 and row['timeout_seconds']==60 and row['duration_seconds']<60,'sanitizer original budget');tap((HERE/'asan/gimp-painter-mybrush.stdout').read_text())
require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',(HERE/'asan/gimp-painter-mybrush.stderr').read_text()),'no sanitizer diagnostic')
b=load('asan/build.json');require(not b['changed_during_build'],'stable sanitizer inputs');require(len(b['instrumented_sources'])==37 and len(b['rtti_compatibility_only_sources'])==40,'bounded instrumentation scope');require(b['executables']['gimp-painter-mybrush']['sha256']==row['executable_sha256'],'sanitizer binary')
for p,h in r['source_sha256'].items():require(b['sources_sha256'][p]==h,'same ordinary/sanitizer source '+p)
gate=load('mypaint-source-contract.json');require(gate['result']=='PASS' and gate['family_assertions']==201 and len(gate['negative_controls'])==6 and all(c['rejected'] for c in gate['negative_controls']),'MyPaint source gate and semantic controls')
full=load('persistence-field-contract.json');require(full['result']=='PASS' and full['counts']['contract_records']==1378 and full['counts']['syntax_candidates']==873 and full['counts']['routes']==16 and len(full['negative_controls'])==46 and all(c['rejected'] for c in full['negative_controls']),'combined source contract')
canonical=next(l for l in (ROOT/'migration/contracts/cpp-c-entry-map.tsv').read_text().splitlines() if l.startswith('gimp_mypaint_brush_save\t'));require(canonical.split('\t')[-1].startswith('PARTIAL:'),'remaining semantics retained')
m=load('artifact-sha256.json');actual={p.relative_to(HERE).as_posix() for p in HERE.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts};require(set(m)==actual,'recursive artifact set')
for p,h in m.items():require(sha(HERE/p)==h,'artifact '+p)
print('PASS: live preview 2 genuine reds; normal42 and bounded sanitizer42; no skips; original60s; source/artifact identities; parent/canonical remain open')

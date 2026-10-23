#!/usr/bin/env python3
"""Check bounded recorded proof; does not execute native tests."""
import hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def load(n):return json.loads((HERE/n).read_text())
def require(v,w):
    if not v:raise SystemExit('FAIL: '+w)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def tap(t):
    lines=t.splitlines();require('1..49' in lines and sum(l.startswith('ok ') for l in lines)==49,'full49 TAP')
    require(not any(l.startswith('not ok ') or re.search(r'(?i)#\s*(skip|todo)\b',l) for l in lines),'no failed/skipped cases')
    for name in ('transitions','preconditions','admission','reentry','retained-lifetime','constructor','handle-and-throwing-hooks'):
        require(any(l.startswith('ok ') and l.endswith('/nested-use/'+name) for l in lines),'new case '+name)
rows=load('normal.json');require(len(rows)==3 and all(r['result']=='OK' and r['returncode']==0 for r in rows),'native/header outcomes')
for name in ('normal.json','restored.json'):
    row=next(r for r in load(name) if r['name'].endswith('/ gimp-painter-mybrush'));require(row['result']=='OK' and row['duration']<60,'ordinary original budget');tap(row['stdout'])
r=load('ordinary-receipt.json');require(r['test_budget_seconds']==60 and len(r['linked_symbols'])==2,'budget and exact two linked symbols')
require(any('-std=c11' in c and 'test-gimp-painter-mybrush.c' in c for c in r['compile_commands']),'real C11 caller');require(any('-std=c++14' in c and 'test-painter-mybrush-duplicate-handle.cpp' in c for c in r['compile_commands']),'real C++14 owner')
for p,h in r['source_sha256'].items():require(sha(ROOT/p)==h,'tested source '+p)
policy=r['build_policy'];require(policy['optimization']==2 and policy['debug'] is False and policy['assertions_enabled'] and not policy['debug_or_assert_disable_flags_found'],'isolated build assertions preserved')
red=load('threshold-control.json');require(len(red)==1 and red[0]['result']=='FAIL' and red[0]['returncode']!=0 and 'assertion failed' in red[0]['stdout'] and 'b->events->str' in red[0]['stdout'],'actual threshold mutation rejected')
m=load('threshold-receipt.json');require(m['original_provider_sha256']==r['source_sha256']['app/core/gimppaintermybrush.cpp'],'mutation base identity')
s=(ROOT/'app/core/gimppaintermybrush.cpp').read_text();require(s.count('return ++impl.use_count == 1;')==1,'exact transition site');require(hashlib.sha256(s.replace('return ++impl.use_count == 1;','return ++impl.use_count == 2;').encode()).hexdigest()==m['mutated_provider_sha256'],'exact mutation identity')
run=load('asan/results.json');require(not run['leak_detection'] and not run['whole_application_instrumented'],'bounded sanitizer disclosure');row=run['results'][0];require(len(run['results'])==1 and row['exit_code']==0 and row['tap_ok']==49 and row['timeout_seconds']==60 and row['duration_seconds']<60,'sanitizer full49 original budget');tap((HERE/'asan/gimp-painter-mybrush.stdout').read_text());require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',(HERE/'asan/gimp-painter-mybrush.stderr').read_text()),'no sanitizer diagnostic')
b=load('asan/build.json');require(not b['changed_during_build'] and len(b['instrumented_sources'])==37 and len(b['rtti_compatibility_only_sources'])==38,'stable bounded instrumentation');require(b['executables']['gimp-painter-mybrush']['sha256']==row['executable_sha256'],'sanitizer binary identity')
require(set(b['excluded_unlinked_rtti_sources'])=={'app/httpd/httpd-navigation.cpp','app/painter-filter-worker.cpp'},'two explicitly unlinked RTTI candidates')
for variants in b['excluded_unlinked_rtti_sources'].values():
    require(variants and all(str((Path(b['executables']['gimp-painter-mybrush']['path']).parents[2]/v).resolve()) not in b['ordinary_link_objects'] for v in variants),'excluded variants absent from link graph')
for p,h in r['source_sha256'].items():require(b['sources_sha256'][p]==h,'same ordinary/sanitizer source '+p)
h=load('header-compile-result.json');require(h['status']=='PASS' and h['compiled_probes']==411 and h['c_probes']==195 and h['cpp_probes']==216,'411 actual compiled header probes')
for p in ('app/core/gimppaintermybrush.h','app/core/gimpmypaintbrush.h'):require(h['source_sha256'][p]==r['source_sha256'][p],'exact compiled public header '+p)
g=load('persistence-field-contract.json');require(g['result']=='PASS' and g['counts']['contract_records']==1378 and g['counts']['syntax_candidates']==873 and len(g['negative_controls'])==46 and all(c['rejected'] for c in g['negative_controls']),'source-only persistence gate')
pre=load('preimplementation.json');require(pre['returncode']==1 and pre['matches']=='','missing-entry observation, not runtime red')
for entry in ('gimp_mypaint_brush_begin_use','gimp_mypaint_brush_end_use'):
    line=next(l for l in (ROOT/'migration/contracts/cpp-c-entry-map.tsv').read_text().splitlines() if l.startswith(entry+'\t'));require(line.split('\t')[-1].startswith('PARTIAL:') and 'preview caller integration remains open' in line,'remaining original duties')
m=load('artifact-sha256.json');actual={p.relative_to(HERE).as_posix() for p in HERE.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts};require(set(m)==actual,'recursive artifact set')
for p,h in m.items():require(sha(HERE/p)==h,'artifact '+p)
print('PASS: nested-use explicit C/handle hooks normal49 + sanitizer49, 411 compiled headers, threshold mutation rejected, exact sources/artifacts; preview integration and parent remain open')

#!/usr/bin/env python3
"""Validate pinned historical and bounded current-config header receipts."""
import argparse,hashlib,json,pathlib,sys,tarfile
P=pathlib.Path(__file__).resolve().parent;R=P.parents[3];sys.path.insert(0,str(R/'tools'))
import check_painter_header_registration as checker
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args()
def load(n):return json.loads((P/n).read_text())
checker.check_historical()
red=load('expected-old-checker-red.json');assert red['exit_code']!=0 and 'app/painter/tests/headers/meson.build' in red['stderr']
assert 'stale generated header inputs' in (P/'expected-stale-current.log').read_text()
assert 'Ran 25 tests' in (P/'control-results.log').read_text() and '\nOK\n' in (P/'control-results.log').read_text()
rows=load('current-test-results.json');assert len(rows)==2 and all(r['returncode']==0 and r['result']=='OK' for r in rows)
current=load('current-build.json');assert current['historical_original_checker']=='PASS'
assert current['current_duties']['original_duties']==current['current_duties']['current_duties']==92
b=current['current_build'];assert b['status']=='PASS' and b['http_enabled'] is True
assert b['compiled_probes']==413 and b['c_probes']==196 and b['cpp_probes']==217
assert len(b['compile_vectors'])==len({r['name'] for r in b['compile_vectors']})==413
sup=load('supersession.json');assert sup['status']=='FINAL_GBOOLEAN_HEADER_COMPILE_VERIFIED'
for n,h in sup['first_artifact_sha256'].items():assert hashlib.sha256((P/sup['first_checkpoint']/n).read_bytes()).hexdigest()==h,n
first=load('first-current-checkpoint/current-build.json')['current_build']
assert first['source_sha256']['app/display/gimpdisplayshell-scroll.h']!=b['source_sha256']['app/display/gimpdisplayshell-scroll.h']
seal=load('current-header-input-sha256.json');assert seal==b['source_sha256']
with tarfile.open(P/'current-header-inputs.tar.gz')as t:
 assert len(t.getmembers())==len(seal)==256 and set(t.getnames())==set(seal)
 for m in t.getmembers():assert hashlib.sha256(t.extractfile(m).read()).hexdigest()==seal[m.name]
assert load('scope.json')['runtime_link_abi_platform_claim'] is False
if args.check_source:
 for n,h in {**seal,**load('checker-source-sha256.json')}.items():assert hashlib.sha256((R/n).read_bytes()).hexdigest()==h,n
for n,h in load('artifact-sha256.json').items():assert hashlib.sha256((P/n).read_bytes()).hexdigest()==h,n
print('PASS unchanged historical231/233+18,92 duties; current HTTP413 and25 recovery controls; default current compile not claimed')

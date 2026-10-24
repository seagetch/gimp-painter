#!/usr/bin/env python3
"""Bounded declared-switch default proof; no acquisition or whole-build claim."""
import argparse,hashlib,json,pathlib,re,subprocess,tarfile,tempfile,shutil
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def validate(package=P,commit=None):
    hashes=json.loads((package/'artifact-sha256.json').read_text())
    for p,h in hashes.items():assert sha((package/p).read_bytes())==h,('artifact changed',p)
    m=json.loads((package/'source-manifest.json').read_text())
    with tarfile.open(package/'sources.tar.gz')as t:blobs={n:t.extractfile(n).read()for n in t.getnames()}
    for p,h in m['owned'].items():
        b=subprocess.check_output(['git','show',commit+':'+p],cwd=R)if commit else(R/p).read_bytes()
        assert sha(b)==h and blobs['candidate/'+p]==b,('source drift',p)
    p='app/paint/painter-mypaint-surface/gimp-painter-options.cpp';before=blobs['predecessor/'+p];after=blobs['candidate/'+p]
    old=b'g_param_spec_boolean(s.internal_name,s.displayed_name,nullptr,FALSE,flags)';new=old.replace(b'FALSE',b's.default_value')
    assert before.count(old)==1 and before.replace(old,new)==after,'unrelated Options source change'
    for name,n in [('prior-serializer',1),('green',4),('asan',4),('numeric-baseline',3),('numeric-candidate',3),('ui',1),('full',28)]:
        r=json.loads((package/name/'result.json').read_text());log=(package/name/'run.log').read_text();assert r['exit']==0,(name,r)
        assert len(re.findall(r'^ok \d+',log,re.M))==n,(name,n)
        assert '# SKIP' not in log and 'ERROR: AddressSanitizer' not in log and 'runtime error:' not in log
        assert json.loads((package/name/'linked-before.json').read_text())==json.loads((package/name/'linked-after.json').read_text()),name
    assert json.loads((package/'expected-red.json').read_text())['exit']!=0
    assert "'declared' should be TRUE" in (package/'expected-red.log').read_text()
    assert (package/'green/fixture.cpp').read_bytes()==(package/'asan/fixture.cpp').read_bytes()
    assert (package/'full/fixture.cpp').read_bytes()==blobs['candidate/app/tests/test-gimp-painter-options.cpp']
    assert (package/'full/test-painter-switch-defaults.inc').read_bytes()==blobs['candidate/app/tests/test-painter-switch-defaults.inc']
    numeric=(R/'migration/tests/editor-numeric-lossless/green-cases.inc').read_bytes()
    assert sha(numeric)==m['numeric_cases_sha256']
    for name in ['numeric-baseline','numeric-candidate']:assert numeric in (package/name/'fixture.cpp').read_bytes()
    for v in ['false','true']:
        for kind in ['full','changed']:assert '(painter-settings ' in (package/'prior-serializer'/f'{v}-{kind}.rc').read_text()
    assert 'removing actual native painter-settings causally changes omitted FALSE to reset TRUE' in (package/'green/run.log').read_text()
    print('PASS normal28, scoped ASan4, numeric3+3, UI1; exact one-expression default change; actual prior native JSON preserves FALSE')
def main():
    a=argparse.ArgumentParser();a.add_argument('--commit');a.add_argument('--negative-controls',action='store_true');x=a.parse_args();validate(commit=x.commit)
    if x.negative_controls:
        for target in ['green/run.log','prior-serializer/false-changed.rc','source-manifest.json','ui/reset-portrait.png']:
            with tempfile.TemporaryDirectory()as d:
                p=pathlib.Path(d)/'proof';shutil.copytree(P,p);f=p/target;f.write_bytes(f.read_bytes()+b'changed')
                try:validate(p,x.commit)
                except AssertionError:pass
                else:raise AssertionError(('negative control accepted',target))
        print('PASS 4 artifact/source-record negative controls; causal runtime predecessor and JSON-removal controls separately preserved')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Validate bounded unavailable Filter XCF evidence without rerunning native tests."""
import hashlib,json,re,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent
CASE='/painter-xcf-roundtrip/filter_unknown_xcf_preservation'
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
    if not ok:raise ValueError(msg)
def result(j):
    check(j['exit_code']==0,'failed result')
    check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==[CASE],'wrong case/count')
    check('6 unknown-only public XCF Save/Open rounds;6 explicit failures;zero executor runs;typed and opaque bytes retained' in j['stdout'],'missing matrix result')
    check(len(re.findall(r'^# unknown (typed|opaque) pass',j['stdout'],re.M))==6,'missing scene')
def main():
    manifest=json.loads((P/'artifact-sha256.json').read_text())
    check(set(manifest)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts},'artifact list')
    for p,h in manifest.items():check(sha((P/p).read_bytes())==h,p)
    before=json.loads((P/'source-before.json').read_text());after=json.loads((P/'source-after.json').read_text());san=json.loads((P/'sanitizer.json').read_text());native=json.loads((P/'native.json').read_text())
    result(native);result(san)
    check(before['sha256']==after['sha256'] and not after['changed_after_run'],'source drift')
    check(san['test_command'][-2:]==['-p',CASE] and not san['changed_after_run'],'san drift')
    check(len(san['sources'])==7 and all(x.endswith('.c') for x in san['sources']) and san['instrumented_cpp_rtti'] is False,'scope')
    check(san['sanitizers']==['address','undefined'] and san['leak_detection'] is False,'sanitizer policy')
    sources={}
    with tarfile.open(P/'sources.tar.gz') as t:
        for e in t:
            check(e.isfile() and not Path(e.name).is_absolute() and '..' not in Path(e.name).parts and e.name not in sources,'archive member')
            sources[e.name]=t.extractfile(e).read()
    expected=dict(san['source_sha256']);expected.update(before['sha256'])
    check(set(sources)==set(expected),'archive set')
    for p,h in expected.items():check(sha(sources[p])==h,'source '+p)
    for p,h in json.loads((P/'owned-source-sha256.json').read_text()).items():check(sha(sources[p])==h,'owned '+p)
    fixture=sources['app/tests/test-filter-unknown-xcf.inc'].decode()
    for text in ('rounds,==,6','failures,==,6','==,GIMP_FILTER_LAYER_FAILED','==,0','g_bytes_equal(saved,raw)','g_bytes_equal(saved,opaque)'):
        check(text in fixture,'missing assertion '+text)
    check('filter_new(' not in fixture,'default scope')
    for mutation in (dict(native,exit_code=1),dict(native,stdout=native['stdout'].replace(CASE,CASE+'-wrong')),dict(native,stdout=native['stdout'].replace('# unknown typed pass1:','# omitted typed pass1:'))):
        try:result(mutation)
        except ValueError:pass
        else:raise ValueError('negative result control accepted')
    print(f'PASS native1/san1;6 Save/Open/6 FAILED/zero executor runs;{len(sources)} source seals;7 C-only units;3 negative controls')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Validate bounded Gaussian composition evidence without rerunning native tests."""
import hashlib,json,re,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent
CASE='/gimp-filter-layer/filter_gaussian_attributes_contract'
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
    if not ok:raise ValueError(msg)
def result(j):
    check(j['exit_code']==0,'failed result')
    check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==[CASE],'wrong case/count')
    check('27 actual Gaussian completed-cache scenes;54 ordinary-layer projection comparisons;18 independent Normalcolor checks' in j['stdout'],'missing matrix result')
    check(len(re.findall(r'^# scene\d+ mode=',j['stdout'],re.M))==27,'missing scene')
def main():
    manifest=json.loads((P/'artifact-sha256.json').read_text())
    check(set(manifest)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts},'artifact list')
    for p,h in manifest.items():check(sha((P/p).read_bytes())==h,p)
    before=json.loads((P/'source-before.json').read_text());after=json.loads((P/'source-after.json').read_text());san=json.loads((P/'sanitizer.json').read_text());native=json.loads((P/'native.json').read_text())
    result(native);result(san)
    check(before['sha256']==after['sha256'] and not after['changed_after_run'],'source drift')
    check(san['selected_test_paths']==[CASE] and not san['changed_after_compile'],'san drift')
    check(len(san['instrumented_sources'])==20 and len(san['rtti_compatibility_only_sources'])==53,'scope')
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
    excerpt=json.loads((P/'predecessor-excerpt.json').read_text())
    for path,digest in excerpt['selected_predecessor_sha256'].items():check(before['sha256'][path]==digest,'predecessor '+path)
    fixture=sources['app/tests/test-filter-attributes-contract.inc'].decode()
    for text in ('comparisons,==,54','normal_checks,==,18','scenes,==,27','==,runs','cache,sizeof(cache)','contract_gaussian_filter(image)'):
        check(text in fixture,'missing assertion '+text)
    check('filter_new(' not in fixture,'default scope')
    for mutation in (dict(native,exit_code=1),dict(native,stdout=native['stdout'].replace(CASE,CASE+'-wrong')),dict(native,stdout=native['stdout'].replace('# scene1 mode=','# omitted mode='))):
        try:result(mutation)
        except ValueError:pass
        else:raise ValueError('negative result control accepted')
    print(f'PASS native1/san1;27 scenes/54 comparisons/18 Normal checks;{len(sources)} source seals;20+53 scope;3 negative controls')
if __name__=='__main__':main()

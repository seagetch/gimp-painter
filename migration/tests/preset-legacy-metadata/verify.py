#!/usr/bin/env python3
import hashlib,json,re,struct
from pathlib import Path


def verify(root, source=None):
    read=lambda n:json.loads((root/n).read_text())
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    def count(r):
        assert r['returncode']==0
        assert 'not ok ' not in r['stdout'] and '# SKIP' not in r['stdout']
        return len(re.findall(r'^ok \d+ ',r['stdout'],re.M))
    m=read('manifest.json');assert m['status']=='DOING' and len(m['remaining'])==2
    assert sum(count(x) for x in read('green.json'))==24
    assert count(read('asan-results.json'))==6
    assert sum(count(x) for x in read('portrait.json'))==1
    assert read('red.json')[0]['returncode']!=0
    assert 'Layer presets Folders' in read('red.json')[0]['stdout']
    a=read('asan-build.json');assert len(a['instrumented_sources'])==7 and not a['changed_during_build']
    assert not a['whole_application_instrumented'] and not a['leak_detection']
    for p,h in read('source-sha256.json').items():
        assert sha(root/'owned-before-build'/p)==h
        if source:assert sha(source/p)==h
        if p in a['sources_sha256']:assert a['sources_sha256'][p]==h
    assert struct.unpack('>II',(root/'legacy-prefs-oncanvas-portrait.png').read_bytes()[16:24])==(1080,1920)
    assert m['oncanvas'] and len(read('paused-source-exclusion.json')['sha256'])==5
    for p,h in read('artifact-sha256.json').items():assert sha(root/p)==h,p
    return {'normal':24,'selected_sanitizer':6,'portrait':[1080,1920],'parent':'DOING'}

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parent
    print(json.dumps(verify(root,root.parents[2] if a.check_source else None)))

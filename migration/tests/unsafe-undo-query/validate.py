#!/usr/bin/env python3
import argparse,hashlib,json,re,subprocess,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check_sources(meta,src):
    assert set(src)==set(meta['owned_sha256'])
    for p,h in meta['owned_sha256'].items():assert sha(src[p])==h,p

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--commit');ap.add_argument('--negative-controls',action='store_true');a=ap.parse_args()
    for p,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
    m=json.loads((P/'source.json').read_text());src={p:subprocess.check_output(['git','show',a.commit+':'+p],cwd=R)if a.commit else (R/p).read_bytes()for p in m['owned_sha256']};check_sources(m,src)
    with tarfile.open(P/'sources.tar.gz')as t:
        old={p:t.extractfile('before/'+p).read()for p in src}
        for p in src:
            assert sha(old[p])==m['predecessor_sha256'][p]
            assert old[p]==subprocess.check_output(['git','show',m['base_commit']+':'+p],cwd=R)
            assert t.extractfile('after/'+p).read()==src[p]
    core='app/paint/painter-mypaint-surface/paint-core.cpp'
    # Save predicate body remains byte-for-byte unchanged, not reinterpreted.
    def pending(b):return b.split(b'  static gboolean pending(')[1].split(b'\n  }')[0]
    assert pending(old[core])==pending(src[core])
    gate='app/core/gimpimage-undo.c'
    assert src[gate]==old[gate].replace(b'gimp_image_has_pending_paint (image)',b'gimp_image_has_unsafe_undo (image)')
    h='app/core/gimpimage.h';assert src[h].replace(b'/* Read-only veto for non-cancelable paint/history transitions, not Save. */\ngboolean        gimp_image_has_unsafe_undo        (GimpImage          *image);\n',b'')==old[h]
    expected={'undo-pop-guard':13,'paired-stroke-undo':18,'painter-smudge-ui':26,'gimp-fill-brush':40,'gimp-painter-session':10,'painter-save-continuation':13,'painter-mypaint-tool':27,'paired-stroke-ui':1,'asan-undo-pop-guard':13,'asan-paired-stroke-undo':18,'asan-painter-mypaint-tool':1}
    for n,count in expected.items():
        result=json.loads((P/(n+'-result.json')).read_text());log=(P/(n+'.log')).read_text();assert result['exit']==0,n;assert len(re.findall(r'^ok ',log,re.M))==count,n;assert 'not ok 'not in log,n
    for x in json.loads((P/'headers.json').read_text()):assert x['exit']==0
    mutants=json.loads((P/'mutations.json').read_text());assert len(mutants)==2
    for x in mutants:
        assert x['exit']!=0;assert ('should be TRUE' if x['name']=='old-generic-gate' else 'should be FALSE') in (P/(x['name']+'.log')).read_text()
    assert (P/'plugin-build.exit').read_text().strip()=='0'
    assert (P/'plugin-source-before.sha256').read_bytes()==(P/'plugin-source-after.sha256').read_bytes()
    n=0
    if a.negative_controls:
        for p in [core,gate,h,'app/tools/gimppaintermybrushtool.cpp']:
            bad=dict(src);bad[p]+=b'\n/* unreviewed drift */'
            try:check_sources(m,bad)
            except AssertionError:n+=1
        assert n==4
    print('PASS native148, scoped five-unit sanitizer32, headers4, linked mutants2; source controls',n)
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Read-only metadata/help source successor; never rerun native callbacks."""
import argparse,hashlib,io,json,pathlib,re,struct,subprocess,tarfile
PACKAGE='migration/tests/preset-catalog-predecessor-successor'
PIN='bea5ff8fed42aca8fd65b2aad5ceb20522a3b3bc1b97921c808cd80077d084ce'
sha=lambda b:hashlib.sha256(b).hexdigest()
def verify(root):
    root=pathlib.Path(root);data=(root/PACKAGE/'transition.json').read_bytes();assert sha(data)==PIN,'transition changed';r=json.loads(data)
    for p,h in r['history_sha256'].items():assert sha((root/p).read_bytes())==h,('historical evidence changed',p)
    for name in ['preset-legacy-metadata','preset-dialog-help']:
        result=subprocess.run(['python3',str(root/'migration/tests'/name/'verify.py')],capture_output=True,text=True)
        assert result.returncode==0,('historical evidence invalid',name,result.stderr)
    archived=(root/PACKAGE/'catalog-sources.tar.gz').read_bytes();assert sha(archived)==r['source_archive_sha256']
    with tarfile.open(fileobj=io.BytesIO(archived),mode='r:gz') as t:
        assert all(m.isfile() for m in t.getmembers());assert len(t.getnames())==len(set(t.getnames()))==9
        assert set(t.getnames())==set(r['current_source_sha256'])
        for p,h in r['current_source_sha256'].items():
            assert sha(t.extractfile(p).read())==h
            assert sha((root/p).read_bytes())==h,('unreviewed current source',p)
    read=lambda p:json.loads((root/p).read_text())
    for p,h in r['catalog_receipts'].items():assert sha((root/p).read_bytes())==h,('catalog receipt changed',p)
    prefix='migration/tests/preset-catalog/';catalog=read(prefix+'source-manifest.json')
    for p,h in r['current_source_sha256'].items():assert catalog['complete_source_sha256'][p]==h
    metadata=read('migration/tests/preset-legacy-metadata/source-sha256.json')
    help_source=read('migration/tests/preset-dialog-help/green/source-manifest.json')['owned_sha256']
    # Unchanged causal assertions, not weaker new fixtures standing in for old ones.
    p='app/tests/test-preset-gui-entries.c';assert r['current_source_sha256'][p]==metadata[p]
    p='app/tests/test-layer-presets-ui.c';assert r['current_source_sha256'][p]==help_source[p]
    counts={'preset-catalog':26,'preset-catalog-ui':5,'preset-gui-entries':6}
    for name,normal in [('normal-results.json',True),('sanitizer-results.json',False)]:
        rows=read(prefix+name);assert len(rows)==3
        for row in rows:
            key=row['name'].split(' / ')[-1] if normal else row['target']
            assert row['returncode']==0 and (not normal or row['result']=='OK')
            assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==counts[key]
    rows=read(prefix+'regression-results.json');assert len(rows)==6
    for key,count in [('layer-presets-ui',18),('preset-gui-entries',6)]:
        row=next(x for x in rows if x['name'].split(' / ')[-1]==key)
        assert row['returncode']==0 and row['result']=='OK'
        assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==count
    d=(root/'app/presets/gimppresetdescriptor.c').read_text()
    assert 'N_("Layer Presets"), NULL, "gimp-layer", ""' in d
    assert 'N_("Open layer presets dialog"), "gimp-layer-preset-dialog"' in d
    assert 'g_array_set_clear_func' not in d
    assert struct.unpack('>II',(root/prefix/'selected-catalog-portrait.png').read_bytes()[16:24])==(1080,1920)
    assert '1080x1920x24' in (root/prefix/'portrait.log').read_text()
    assert len(r['remaining'])==2
    return r
if __name__=='__main__':
    local=pathlib.Path(__file__).resolve().parent
    hashes=json.loads((local/'artifacts.json').read_text())
    assert set(hashes)=={p.name for p in local.iterdir() if p.is_file() and p.name!='artifacts.json'}
    for name,h in hashes.items():assert sha((local/name).read_bytes())==h,('package artifact changed',name)
    parser=argparse.ArgumentParser();parser.add_argument('--source-root',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[3]);args=parser.parse_args();verify(args.source_root)
    print('PASS: historical metadata/help preserved; exact catalog37 + native UI18/legacy6 successor; ABI parents remain open')

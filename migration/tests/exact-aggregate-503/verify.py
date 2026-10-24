#!/usr/bin/env python3
"""Read-only exact-source aggregate admission, explicitly NOT all-pass."""
import hashlib,json,pathlib,re,tarfile,struct
COMMIT='503ade3466ddebc60d13d20502ebf0eb0c617f96'
ARCHIVE='e2c03519020f39ca94c3fc144e3f4e7a2f0a40e95587f07205d2dd8801af4f10'
def validate(rows,summary,extras):
    assert len(rows)==42 and sum(r['result']=='OK' for r in rows)==41
    failed=[r for r in rows if r['result']!='OK'];assert len(failed)==1
    assert failed[0]['name']=='gimp:app / painter-smudge-ui'
    text=(failed[0]['stdout'] or '')+(failed[0]['stderr'] or '')
    assert 'undo_pending' in text and 'gimp_image_undo(s.image)' in text
    assert summary['whole_aggregate_pass'] is False and summary['completed_ok_tap_cases']==449
    assert sum(len(re.findall(r'^ok \d+',r['stdout'] or '',re.M)) for r in rows)==449
    assert len(extras)==4 and all(r.get('returncode')==0 for r in extras)
    expected={'red-eye-xcf':3,'red-eye-portrait':1,'filter-selected-no-edge':30}
    for row in extras:
        if row['name'] in expected:assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==expected[row['name']]
    assert summary['extra_graph_registered_cases']==3

def verify(root):
    root=pathlib.Path(root);data=(root/'evidence.tar.gz').read_bytes();assert hashlib.sha256(data).hexdigest()==ARCHIVE
    graph_data=(root/'graph-ui-result.json').read_bytes();assert hashlib.sha256(graph_data).hexdigest()=='c35fc33213a76ac478d0a4954089083c463dd05a1eb1285adc7d1d266ae274d3'
    graph=json.loads(graph_data);assert len(graph)==1 and graph[0]['result']=='OK'
    assert len(re.findall(r'^ok \d+',graph[0]['stdout'],re.M))==3
    prefix='exact-'+COMMIT+'/'
    with tarfile.open(root/'evidence.tar.gz') as t:
        names=t.getnames();assert len(names)==len(set(names))
        assert t.extractfile(prefix+'source-commit.txt').read().decode().strip()==COMMIT
        pngs=[n for n in names if n.endswith('.png')]
        assert pngs
        for n in pngs:
            header=t.extractfile(n).read(24)
            physical={prefix+'physical-screenshots/'+x for x in ('01-before.png','02-released.png','03-undo.png','04-redo.png')}
            expected=(1040,1800) if n in physical else (1080,1920)
            assert header[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',header[16:24])==expected,n
        read=lambda n:json.loads(t.extractfile(prefix+n).read())
        pins=read('artifacts-sha256.json');assert len(pins)==44
        for n,h in pins.items():assert hashlib.sha256(t.extractfile(prefix+n).read()).hexdigest()==h,n
        validate(read('results.json'),read('aggregate-summary.json'),read('extra-ui-results.json'))
        assert read('postrun-source-seal.json')=={'changed':[],'checked_files':18805}
        assert read('postrun-external-data-seal.json')=={'checked_files':1714,'changed':[]}
        held=read('postrun-held-exclusions.json');assert held['held_scopes']==7 and held['providers']==11 and held['all_unchanged'] and held['untracked_size_domain_candidate_absent']
        source=read('prebuild-tracked-source-sha256.json');assert len(source)==18805
        scope=read('seven-hold-source-exclusion-successors.json');assert scope['held_scope_count']==7
        for row in scope['rows']:assert source[row['path']]==row['current_sha256']
        s=scope['new_smudge_hold'];assert source[s['path']]==s['sha256'] and s['git_blob']=='40c57e21ae07ffb4cc2500977e4033b983750d59'
        assert not any(n.endswith('test-painter-smudge-size-domain.inc') for n in source)
        assert len(read('native-executable-sha256.json'))==40
        assert b'1080x1920x24, authenticated accepted, empty authority rejected' in t.extractfile(prefix+'tests.log').read()
        assert t.extractfile(prefix+'catalog-oncanvas-portrait.png').read(8)==b'\x89PNG\r\n\x1a\n'
    return True
if __name__=='__main__':
    verify(pathlib.Path(__file__).resolve().parent)
    print('VERIFIED: exact503 41/42 targets; Smudge UI Undo failure retained;37 selected extras pass; NOT aggregate PASS')

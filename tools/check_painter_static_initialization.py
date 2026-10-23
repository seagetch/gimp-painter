#!/usr/bin/env python3
"""Verify the source-matched original 04.018 native evidence checkpoint."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/static-initialization'


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status']=='PASS' and report['task']=='04.018'
    for path,digest in report['source_sha256'].items():
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest, path
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    path = DIRECTORY/'evidence.tar.gz'
    assert hashlib.sha256(path.read_bytes()).hexdigest()==report['evidence_sha256']
    with tarfile.open(path) as archive:
        members=archive.getmembers();data={}
        assert len(members)==len(manifest)==report['evidence_members']
        assert len({m.name for m in members})==len(members)
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute()
            assert '..' not in Path(member.name).parts
            content=archive.extractfile(member).read();content.decode('utf-8')
            assert len(content)==manifest[member.name]['size']
            assert hashlib.sha256(content).hexdigest()==manifest[member.name]['sha256']
            data[member.name]=content
    for variant,count in [('default',73),('http',78)]:
        native=json.loads(data[variant+'-object-initializers.json'])
        assert native['status']=='PASS' and native['object_count']==count and native['findings']==0
        assert len(native['production_inputs'])==count
        assert len({r['source'] for r in native['production_inputs']})==count
        assert native['http_enabled']==(variant=='http')
        assert not any(r['findings'] for r in native['objects'])
        closure=json.loads(data[variant+'-input-closure.json'])
        for p,digest in closure['source_sha256'].items():
            assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest,p
        assert len(closure['final_links'])==3
        assert not any(r['explicit_init_override'] for r in closure['final_links'])
    controls=json.loads(data['controls/report.json'])
    assert controls['status']=='PASS' and len(controls['cases'])==8
    assert all(r['expected_rejected']==r['observed_rejected'] for r in controls['cases'])
    scope=json.loads(data['scope-controls/report.json'])
    assert scope['status']=='PASS' and len(scope['results'])==9 and all(r['passed'] for r in scope['results'])
    with (ROOT/'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task']=='04.018' for r in csv.DictReader(stream,delimiter='\t'))
    print('PASS: original04.018 current sources, 73/78 real objects, startup and coverage controls')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Validate source-matched original 04.015 production incremental evidence."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/incremental-build'


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status'] == 'PASS'
    for path, digest in report['source_sha256'].items():
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    with tarfile.open(DIRECTORY/'evidence.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == 39
        assert len({m.name for m in members}) == len(members)
        data = {}
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute()
            assert '..' not in Path(member.name).parts
            content = archive.extractfile(member).read(); content.decode('utf-8')
            assert len(content) == manifest[member.name]['size']
            assert hashlib.sha256(content).hexdigest() == manifest[member.name]['sha256']
            data[member.name] = content
    measured = json.loads(data['runtime-report.json'])
    expected = json.loads(data['expected-inputs.json'])
    assert measured['status'] == 'PASS' and measured['source_headers_restored']
    assert measured['tracked_source_unchanged'] and len(measured['trials']) == 4
    pairs = set()
    for row in measured['trials']:
        assert row['status'] == 'PASS'
        key = (row['variant'],row['header']); assert key not in pairs; pairs.add(key)
        wanted = expected[row['variant']]['expected_trials'][row['header']]
        assert row['expected'] == wanted
        for kind in ['objects','archives','final_bins']:
            actual = sorted(p for p,v in row['before'][kind].items()
                            if v != row['after'][kind][p])
            assert actual == row['changed'][kind]
            assert set(actual) == set(wanted['expected_'+kind])
            assert set(row['restore_changes'][kind]) == set(wanted['expected_'+kind])
            assert not row['noop_changes'][kind] and not row['restored_noop_changes'][kind]
        dependency = json.loads(data['dependencies/'+row['variant']+'-'+Path(row['header']).stem+'.json'])
        assert dependency['consumers'] == wanted['expected_objects']
        for key in ['build','noop','restore','restored_noop']:
            operation = row[key]; assert operation['exit_code'] == 0
            assert hashlib.sha256(data['native/'+operation['log']]).hexdigest() == operation['log_sha256']
    assert pairs == {(v,h) for v in ['default','http'] for h in measured['headers']}
    for variant in ['default','http']:
        final = data['native/'+variant+'-final-noop.log'].decode()
        assert 'Compiling ' not in final and 'Linking ' not in final
    review = json.loads((DIRECTORY/'source-duty-review.json').read_text())
    with (ROOT/'migration/inventory/legacy-port-work-items.tsv').open() as source:
        duties = {r['work_id']:r for r in csv.DictReader(source,delimiter='\t') if r['wbs_task']=='04.015'}
    assert len(duties) == len(review['duties']) == 28
    for record in review['duties']:
        duty = duties[record['work_id']]
        assert duty['phase']=='verification' and duty['status']=='DONE'
        assert all(duty[k]==record[k] for k in ['source_id','path','source_blob','source_range','source_sha256'])
        for path,digest in record['current_source_sha256'].items():
            assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,path
    print('PASS: exact production incremental sets/no-ops and all28 original verification duties')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Verify original 04.017 generated-header source, ABI evidence and duty coverage."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/generated-headers'


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status'] == 'PASS' and report['task'] == '04.017'
    for path, digest in report['source_sha256'].items():
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    archive_path = DIRECTORY/'evidence.tar.gz'
    assert hashlib.sha256(archive_path.read_bytes()).hexdigest() == report['evidence_sha256']
    with tarfile.open(archive_path) as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == report['evidence_members']
        assert len({m.name for m in members}) == len(members)
        data = {}
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute()
            assert '..' not in Path(member.name).parts
            content = archive.extractfile(member).read()
            content.decode('utf-8')
            expected = manifest[member.name]
            assert len(content) == expected['size']
            assert hashlib.sha256(content).hexdigest() == expected['sha256']
            data[member.name] = content
    for variant, count in [('default',357), ('http',359)]:
        native = json.loads(data[variant+'-header-compile-result.json'])
        assert native['status'] == 'PASS' and native['compiled_probes'] == count
        assert native['c_probes'] == 169 and len(native['generated_source_targets']) == 63
        assert not native['outer_linkage_adapter']
        for path, digest in native['source_sha256'].items():
            assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
        actual = json.loads(data['build-header-probes/'+variant+'/report.json'])
        assert actual['status'] == 'passed' and not actual['failures'] and actual['inputs_stable']
        assert len(actual['headers']) == 42 and len(actual['abi']) == 18
        assert all(h['double_include'][lang]['passed'] for h in actual['headers'] for lang in ('c','cpp'))
        assert all(a['linked'] for a in actual['abi'])
        assert sum(a.get('executed',False) for a in actual['abi']) == 17
        assert all(a.get('tested_symbols_resolved') for a in actual['abi'] if a['link']=='relocatable')
        assert len(actual['runtime']) == 2 and all(r['executed'] for r in actual['runtime'])
        assert len(actual['negative_controls']) == 2 and all(c['rejected'] for c in actual['negative_controls'])
    controls = json.loads(data['controls/controls-result.json'])['results']
    assert len(controls) == 23 and all(c['observed_expected_result'] for c in controls)
    assert json.loads(data['resource-wrapper/verification.json'])['tests_passed'] == 8
    review = json.loads((DIRECTORY/'source-duty-review.json').read_text())
    with (ROOT/'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        rows = list(csv.DictReader(stream, delimiter='\t'))
    duties = {r['work_id']:r for r in rows if r['wbs_task']=='04.017'}
    assert len(duties) == len(review['duties']) == 89
    for record in review['duties']:
        duty = duties[record['work_id']]
        assert duty['status'] == 'DONE' and duty['phase'] == 'verification'
        assert all(record[k] == duty[k] for k in ('source_id','path','source_blob','source_range','source_sha256'))
        assert {p['language'] for p in record['compiled_probes']} == {'c','cpp'}
        for path,digest in record['current_source_sha256'].items():
            assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
    print('PASS: 105 generated headers, current C/C++/real-C ABI evidence, all 89 original duties')


if __name__ == '__main__':
    main()

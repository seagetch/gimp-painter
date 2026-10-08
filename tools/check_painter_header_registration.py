#!/usr/bin/env python3
"""Verify source-matched original 04.014 compilation and duty evidence."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/header-registration'


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status'] == 'PASS'
    for path, digest in report['source_sha256'].items():
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    with tarfile.open(DIRECTORY/'evidence.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == 110
        assert len({member.name for member in members}) == len(members)
        data = {}
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute()
            assert '..' not in Path(member.name).parts
            content = archive.extractfile(member).read(); content.decode('utf-8')
            expected = manifest[member.name]
            assert len(content) == expected['size']
            assert hashlib.sha256(content).hexdigest() == expected['sha256']
            data[member.name] = content
    for variant, count, private in [('default',231,19), ('http',233,21)]:
        result = json.loads(data['native/'+variant+'-final-result.json'])
        assert result['status'] == 'PASS' and result['compiled_probes'] == count
        assert result['c_probes'] == 106 and result['cpp_probes'] == count-106
        assert len(result['public_targets']) == 102
        assert len(result['private_cpp_targets']) == private
        assert not result['outer_linkage_adapter']
        for path, digest in result['source_sha256'].items():
            assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest, path
    controls = json.loads(data['controls/controls-result.json'])
    assert len(controls['results']) == 18
    assert all(row['observed_expected_result'] for row in controls['results'])
    review = json.loads((DIRECTORY/'source-duty-review.json').read_text())
    with (ROOT/'migration/inventory/legacy-port-work-items.tsv').open() as source:
        rows = list(csv.DictReader(source, delimiter='\t'))
    duties = {row['work_id']:row for row in rows if row['wbs_task']=='04.014'}
    assert len(duties) == len(review['duties']) == 92
    for record in review['duties']:
        duty = duties[record['work_id']]
        assert duty['status'] == 'DONE' and duty['phase'] == 'verification'
        assert all(record[key] == duty[key] for key in ['source_id','path','source_sha256'])
        assert record['current_headers']
    # This checkpoint's unrelated execution state is intentionally historical.
    unrelated = [row for row in rows if row['wbs_task']!='04.014']
    assert len(unrelated) == review['unrelated_rows']
    assert hashlib.sha256(json.dumps(unrelated,sort_keys=True,ensure_ascii=False).encode()).hexdigest() == review['unrelated_rows_sha256']
    print('PASS: registered native C/C++ header probes and all92 original verification duties')


if __name__ == '__main__':
    main()

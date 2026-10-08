#!/usr/bin/env python3
"""Validate original 04.016's source-matched installed startup evidence."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/installed-startup'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status'] == 'PASS' and report['task'] == '04.016'
    for path, expected in report['source_sha256'].items():
        assert digest((ROOT/path).read_bytes()) == expected, path
    assert digest((DIRECTORY/'evidence.tar.gz').read_bytes()) == report['evidence_sha256']
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    data = {}
    with tarfile.open(DIRECTORY/'evidence.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == report['evidence_members']
        for member in members:
            assert member.isfile() and member.name not in data
            assert not Path(member.name).is_absolute() and '..' not in Path(member.name).parts
            value = archive.extractfile(member).read()
            if member.name.endswith('.jpg'):
                assert value.startswith(b'\xff\xd8')
            else:
                value.decode('utf-8')
            assert len(value) == manifest[member.name]['size']
            assert digest(value) == manifest[member.name]['sha256']
            data[member.name] = value
    packages = json.loads(data['package/summary.json'])
    assert len(report['runs']) == 6
    pairs = set()
    for run in report['runs']:
        variant, kind = run['variant'], run['kind']
        assert (variant, kind) not in pairs
        pairs.add((variant, kind))
        observed = json.loads(data[run['source_report']])
        assert observed['status'] == 'passed' and observed['exit_code'] == 0 and not observed['errors']
        if kind == 'filter':
            assert observed['observed_helpers'] and observed['observed_plugins']
            assert not observed['survivors'] and not observed['leftover_profiles'] and not observed['wrong_installation']
            assert observed['batch_result']['status'] == 'passed'
            continue
        prefix = 'runtime/'+variant+'-'+kind+'/'
        assert digest(data[prefix+'probe.executed.py']) == observed['probe_sha256']
        assert digest(data[prefix+'startup.log']) == observed['log_sha256']
        assert not observed['diagnostics'] and observed['development_paths_absent']['/workspace']
        assert all(observed['development_paths_absent'].values())
        log = data[prefix+'startup.log'].decode()
        assert 'INIT: gimp_real_restore' in log and 'EXIT: gimp_finalize' in log
        identifiers = [e.get('identifier', '') for e in ET.fromstring(data[prefix+'tags.xml'])]
        for directory, key in [('painter-mypaint-brushes', 'painter_brushes'), ('layer-presets', 'layer_presets')]:
            actual = sorted(s.removeprefix('external:${gimp_data_dir}/').split('//')[0]
                            for s in identifiers if s.startswith('external:${gimp_data_dir}/'+directory+'/'))
            assert actual == packages[variant][key]
            assert all('  Loading /runtime 日本語/usr/share/gimp/3.0/'+p in log for p in actual)
        if kind == 'gui':
            assert len(packages[variant]['modules']) == 9
            assert sorted(Path(p).name for p in observed['loaded_modules']) == packages[variant]['modules']
            toolrc = data[prefix+'toolrc'].decode()
            assert len(run['host_tools']) == 4
            assert all('(GimpToolInfo "'+name+'"' in toolrc for name in run['host_tools'])
            if variant == 'default':
                observation = json.loads(data[prefix+'observation.json'])
                assert observation['run_id'] == observed['run_id']
                assert observed['image_name'] in observation['windows'][0]['title']
                assert len(observed['user_resources']) == 2
    assert pairs == {(v, k) for v in ['default', 'http'] for k in ['console', 'gui', 'filter']}
    for variant, expected in [('default', 5057), ('http', 5059)]:
        assert packages[variant]['before_after_recipe_runtime_identical']
        assert packages[variant]['runtime_files'] == expected
    control = json.loads(data['controls/package/validation.json'])
    assert control['exit_code'] == 0 and control['source_unchanged']
    assert control['source_sha256_before'] == control['source_sha256_after']
    for path, expected in control['source_sha256_after'].items():
        assert digest((ROOT/path).read_bytes()) == expected
    assert digest(data['controls/package/output.log']) == control['output_sha256']
    assert b'Ran 60 tests' in data['controls/package/output.log'] and data['controls/package/output.log'].rstrip().endswith(b'OK')
    assert b'Ran 5 tests' in data['controls/startup-guard-tests.log']
    assert json.loads(data['controls/negative-missing-brushes-control.json'])['passed']
    review = json.loads((DIRECTORY/'source-duty-review.json').read_text())
    work = {r['work_id']: r for r in csv.DictReader((ROOT/'migration/inventory/legacy-port-work-items.tsv').open(), delimiter='\t')}
    duties = {key: row for key, row in work.items() if row['wbs_task'] == '04.016'}
    assert len(duties) == len(review['duties']) == 39 and not review['assignments_changed']
    for record in review['duties']:
        duty = duties[record['work_id']]
        assert duty['status'] == 'DONE' and duty['phase'] == 'verification'
        assert all(duty[key] == record[key] for key in ['source_id', 'path', 'source_blob', 'source_range', 'source_sha256'])
        for path, expected in record['current_source_sha256'].items():
            assert digest((ROOT/path).read_bytes()) == expected, path
        for remaining in record['remaining_implementation_duties']:
            assert work[remaining['work_id']]['phase'] == 'implementation'
            assert work[remaining['work_id']]['wbs_task'] == remaining['task']
    print('PASS: isolated installed console/GUI/Filter routes and all 39 original verification duties')


if __name__ == '__main__':
    main()

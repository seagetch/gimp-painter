#!/usr/bin/env python3
"""Verify the source-matched original 04.019 audit and native result checkpoint."""
import csv
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / 'migration/tests/allocator-ownership'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    report = json.loads((DIRECTORY / 'report.json').read_text())
    assert report['task'] == '04.019' and report['status'] == 'PASS'
    inventory = json.loads((DIRECTORY / 'source-inventory.json').read_text())
    assert sha((DIRECTORY / 'source-inventory.json').read_bytes()) == report['inventory_sha256']
    units = inventory['production_cpp_translation_units']
    assert len(units) == len({row['source'] for row in units}) == 78
    for path, digest in inventory['source_sha256'].items():
        assert sha((ROOT / path).read_bytes()) == digest, path
    for row in units:
        assert row['sha256'] == inventory['source_sha256'][row['source']]
    archive_path = DIRECTORY / 'evidence.tar.gz'
    assert sha(archive_path.read_bytes()) == report['evidence_sha256']
    manifest = json.loads((DIRECTORY / 'evidence-manifest.json').read_text())
    data = {}
    with tarfile.open(archive_path) as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == report['evidence_members']
        assert len({m.name for m in members}) == len(members)
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute()
            assert '..' not in Path(member.name).parts
            content = archive.extractfile(member).read()
            content.decode('utf-8')
            assert len(content) == manifest[member.name]['size']
            assert sha(content) == manifest[member.name]['sha256']
            data[member.name] = content
    def js(name):
        return json.loads(data[name])
    http = js('http-faults/summary.json')
    assert http['baseline']['summary']['leak_cases'] == 6
    for name in ('fixed', 'fixed-asan-ubsan', 'fixed-asan-ubsan-native-new'):
        result = http[name]['summary']
        assert result['ok'] and result['leak_cases'] == 0 and not result['failures']
        assert result['process_cases'] == (6 if name.endswith('native-new') else 29)
        assert http[name]['source_sha256'] == inventory['source_sha256']['app/httpd/httpd-resource.cpp']
    for prefix in ('', '-sanitized'):
        for version, leaks in (('baseline', 6), ('fixed', 0)):
            text = data['dispatcher-faults/logs/' + version + '-fault' + prefix + '.log'].decode()
            assert ('PASS observations=20 failures=13 successes=7 observed_ref_leaks=%d ' % leaks) in text
    error = js('error-faults/run-results.json')
    assert len(error['results']) == 6 and all(r['passed'] for r in error['results'])
    assert error['results'][-1]['expected_failure']
    assert 'alloc-dealloc-mismatch' in error['results'][-1]['output']
    assert 'source_null=1 freed=1 live_errors=0' in error['results'][1]['output']
    assert all(r['exit_code'] == 0 for r in js('final-native-results.json'))
    for name, count in (('foundation', 40), ('fair-dispatcher', 8), ('http', 17)):
        log = data[name + '-final-native.log'].decode()
        assert len([line for line in log.splitlines() if line.startswith('ok ')]) == count
    san = js('foundation-sanitizers/report.json')
    assert san['exit_code'] == 0 and san['alloc_dealloc_mismatch']
    assert san['leak_sanitizer'] is False and san['vptr'] is False
    for path, digest in san['source_sha256'].items():
        assert sha((ROOT / path).read_bytes()) == digest, path
    for variant, count in (('default', 357), ('http', 359)):
        headers = js(variant + '-header-compile-result.json')
        assert headers['status'] == 'PASS' and headers['compiled_probes'] == count
        assert not headers['outer_linkage_adapter']
        for path, digest in headers['source_sha256'].items():
            assert sha((ROOT / path).read_bytes()) == digest, path
    for result in js('boundary-regression-results.json'):
        # Initial no-display result is superseded by the actual full native run.
        assert result['exit_code'] == (77 if result['name'] == 'painter-profile-native' else 0)
    profile = js('profile-diagnosis/report.json')
    assert profile['original_suite']['returncode'] == 0
    assert profile['original_suite']['passed_tests'] == 9
    assert report['allocator_family_mismatches_found'] == 0
    assert report['normal_delete_leak_hypothesis'] == 'disproved; prompt change reverted'
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '04.019' for r in csv.DictReader(stream, delimiter='\t'))
    print('PASS: original04.019 allocator families, current source seals, native and fault-injection evidence')


if __name__ == '__main__':
    main()

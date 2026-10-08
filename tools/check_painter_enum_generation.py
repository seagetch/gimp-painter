#!/usr/bin/env python3
"""Verify sealed original 04.013 generation evidence and its source identities."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'migration/tests/enum-generation'


def main():
    report = json.loads((DIRECTORY/'report.json').read_text())
    assert report['status'] == 'PASS'
    for name, expected in report['source_sha256'].items():
        assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected, name
    manifest = json.loads((DIRECTORY/'evidence-manifest.json').read_text())
    with tarfile.open(DIRECTORY/'evidence.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == 183
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
    spec = importlib.util.spec_from_file_location('enum_probe', DIRECTORY/'probe.py')
    probe = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(probe)
    results = json.loads(data['isolated/results.json'])
    assert set(results) == {'baseline', 'fixed'}
    for variant, cases in results.items():
        assert len(cases) == 8 and all(case['immediate_noop'] for case in cases)
        probe.validate_results(cases, variant == 'fixed')
    for path in ['native/native-graph-report.json', 'native/native-header-rebuild.json',
                 'native/brush-settings.json', 'native/routing.json']:
        assert json.loads(data[path])['status'] == 'PASS', path
    native = json.loads(data['native/native-header-rebuild.json'])
    assert native['source_restored'] and len(native['variants']) == 2
    for variant in native['variants']:
        assert variant['both_c_and_cpp_rebuilt'] and variant['source_generation_stamp_rebuilt']
        assert not variant['noop_rebuilt_objects']
    assert json.loads(data['native/public-mode-abi.log'])['passed']
    assert 'Ok:                 3' in data['native/parent-brush-tests.log'].decode()
    print('PASS: sealed source-matched enum/PDB generation, C/C++ rebuild and no-op evidence')


if __name__ == '__main__':
    main()

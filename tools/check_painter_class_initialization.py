#!/usr/bin/env python3
"""Verify the class-init exception-path removal and native registration evidence."""
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]
D = ROOT / 'migration/tests/class-initialization'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    report = json.loads((D / 'report.json').read_text())
    assert report['task'] == '05.013/class-init-error' and report['status'] == 'PASS'
    for path, digest in report['source_sha256'].items():
        assert sha((ROOT / path).read_bytes()) == digest, path
    spec = importlib.util.spec_from_file_location('vfunc', ROOT / 'tools/check_painter_vfunc_order.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    current = module.discover()
    assert current == json.loads((D / 'initializer-inventory.json').read_text())
    assert len(current) == 31 and sum(len(t['initializers']) for t in current.values()) == 38
    previous = {t['type']: t for t in json.loads((ROOT / 'migration/contracts/cpp-vfunc-order.json').read_text())['types']}
    for name, row in current.items():
        old = previous[name]
        assert row['source'] == old['source'] and row['parent_macro'] == old['parent_macro']
        assert [(i['symbol'], i['assigned_fields'], i['assigned_callbacks']) for i in row['initializers']] == [
            (i['symbol'], i['assigned_fields'], i['assigned_callbacks']) for i in old['initializers']], name
    source = (ROOT / 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp').read_text()
    assert 'property_name' not in source
    for kind in ('double', 'boolean', 'string'):
        assert 'g_param_spec_' + kind + '(s.internal_name,' in source
    for path in (ROOT / 'app').rglob('*'):
        if path.is_file() and path.suffix in ('.cpp', '.cc', '.hpp') and 'tests' not in path.parts:
            assert not re.search(r'\b(?:GClassWrapper|WithClass|NewGClass|InvalidClass|with_class)\b|\bthrow\s+new\b', module.clean(path.read_text())), str(path)
    manifest = json.loads((D / 'evidence-manifest.json').read_text())
    assert sha((D / 'evidence.tar.gz').read_bytes()) == report['evidence_sha256']
    data = {}
    with tarfile.open(D / 'evidence.tar.gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(manifest) == report['evidence_members']
        assert len({m.name for m in members}) == len(members)
        for member in members:
            assert member.isfile() and not Path(member.name).is_absolute() and '..' not in Path(member.name).parts
            raw = archive.extractfile(member).read(); raw.decode('utf-8')
            assert len(raw) == manifest[member.name]['size'] and sha(raw) == manifest[member.name]['sha256']
            data[member.name] = raw
    summary = json.loads(data['native/summary.json'])
    assert summary['subprocesses'] == 60 and summary['controlled_baseline_faults'] == 44
    assert summary['baseline_partial_retry_outcomes'] == 2 and summary['unexpected_outcomes'] == 0 and summary['all_expected_outcomes']
    results = json.loads(data['native/native-results.json'])
    assert len(results) == 60 and not any(r['timed_out'] for r in results)
    assert sum(r['exit_code'] == 10 for r in results) == 44
    for row in results:
        if row['variant'].startswith('fixed'):
            assert row['exit_code'] == 0, row
            if row['arguments'][0] in ('0', '1'):
                assert 'cpp_allocations=0 properties=126 own=55 inherited=71' in row['output']
        assert data['native/' + row['log']].decode() == row['output']
    native = json.loads(data['native-options-result.json'])
    assert native['exit_code'] == 0 and native['passed'] == 10
    legacy = data['legacy/legacy-glib-cxx-impl.hpp']
    assert hashlib.sha1(b'blob ' + str(len(legacy)).encode() + b'\0' + legacy).hexdigest() == 'a2ad35e37a3e7a481698c1f8b78b71adb385f7d3'
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '05.013/class-init-error' for r in csv.DictReader(stream, delimiter='\t'))
    print('PASS: class-init-error 31 types/38 initializers;60 native probes and10 options cases')


if __name__ == '__main__':
    main()

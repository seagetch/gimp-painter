"""Bounded error-propagation successor; no persisted field or grammar change."""
import hashlib
import io
import json
from pathlib import Path
import tarfile

PACKAGE = 'migration/tests/persistence-delete-successor'
PATH = 'app/tools/gimp-tools.c'
PINS = {
    'transition.json': '082367097482b635086d5dc77f0a2fae4956fa9655f5c41a20a8880a082bfa76',
    'sources.tar.gz': '96a6d45cbd04a2b9f0c85a8632ef715aef07c388e9e21b57029d647d6656df26',
}
BEFORE = b'gimp_tool_options_delete (tool_info->tool_options, NULL)'
AFTER = b'gimp_tool_options_delete (tool_info->tool_options, error)'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def exact_change(before, after):
    assert before.count(BEFORE) == 1, 'delete predecessor call differs'
    assert before.replace(BEFORE, AFTER) == after, 'not the exact one-line error propagation'


def reviewed(root):
    root = Path(root)
    blobs = {name: (root / PACKAGE / name).read_bytes() for name in PINS}
    for name, data in blobs.items():
        assert sha(data) == PINS[name], ('delete successor proof changed', name)
    record = json.loads(blobs['transition.json'])
    assert record['path'] == PATH
    with tarfile.open(fileobj=io.BytesIO(blobs['sources.tar.gz']), mode='r:gz') as tar:
        assert tar.getnames() == ['predecessor.c', 'successor.c']
        assert all(m.isfile() for m in tar.getmembers())
        before = tar.extractfile('predecessor.c').read()
        after = tar.extractfile('successor.c').read()
    assert sha(before) == record['predecessor_sha256']
    assert sha(after) == record['successor_sha256']
    exact_change(before, after)
    assert (root / PATH).read_bytes() == after, 'unreviewed tools source drift'
    proof = {path: (root / path).read_bytes() for path in record['proof']}
    for path, data in proof.items():
        assert sha(data) == record['proof'][path], ('delete runtime receipt changed', path)
    package = 'migration/tests/tool-options-delete-result/'
    report = json.loads(proof[package + 'results.json'])
    assert report['source_sha256']['before-tools'] == sha(before)
    assert report['source_sha256']['after-tools'] == sha(after)
    assert report['clean_source_changed'] == report['linked_inputs_changed'] == []
    runs = {r['name']: r for r in report['results'] if r['name'].endswith('-run')}
    assert set(runs) == {'red-run', 'normal-run', 'asan-run'}
    assert runs['red-run']['returncode'] != 0
    assert "'error' should not be nullptr" in runs['red-run']['stdout']
    for name in ('normal-run', 'asan-run'):
        assert runs[name]['returncode'] == 0
        assert sum(s.startswith('ok ') for s in runs[name]['stdout'].splitlines()) == 10
        assert 'caller-permission' in runs[name]['stdout'] and 'SKIP' not in runs[name]['stdout']
    with tarfile.open(fileobj=io.BytesIO(proof[package + 'tested-sources.tar.gz']), mode='r:gz') as tar:
        assert tar.extractfile('gimp-tools-before.c').read() == before
        assert tar.extractfile('gimp-tools-after.c').read() == after
    return record


def resolve_source(root, previous, actual):
    record = reviewed(root)
    assert previous['path'] == PATH and previous['revision'] == 'native'
    assert previous['successor_sha256'] == record['predecessor_sha256'], 'delete chain predecessor differs'
    assert actual == record['successor_sha256'], 'delete chain successor differs'
    return dict(previous, successor_sha256=actual,
                intermediate_sha256=previous['successor_sha256'],
                commits=previous['commits'] + [record['successor_commit']],
                semantic_successor='one-line delete error propagation; serialization unchanged',
                runtime_proof=record['proof'])

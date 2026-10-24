"""Exact reviewed source successors; original persistence field seals stay intact.

This is a source metadata admission, never a runtime compatibility assertion.
Unknown drift is rejected, including changes to otherwise reviewed files.
"""
import hashlib
import io
import json
from pathlib import Path
import tarfile

PACKAGE = 'migration/tests/persistence-source-successor'
PINS = {
    'transitions.json': '8a143c68e0d4567516e3ed5029088b1b80e9372801b3b5f8cf376bcd524cec11',
    'source-transitions.tar.gz': 'e77961d3986500d5f1f4d9ce9e506be93b25bf74139d136aa8aca78d2b53911c',
    'predecessor-ledgers.tar.gz': '692cee27e479eaf624f94a8340ea8d9fbe6b500019badf1012d614a52cf4f4fd',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def members(data):
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        records = archive.getmembers()
        assert all(m.isfile() for m in records), 'non-file proof member'
        assert len({m.name for m in records}) == len(records), 'duplicate proof member'
        return {m.name: archive.extractfile(m).read() for m in records}


def reviewed(root):
    root = Path(root)
    blobs = {name: (root / PACKAGE / name).read_bytes() for name in PINS}
    for name, data in blobs.items():
        assert digest(data) == PINS[name], ('successor proof changed', name)
    table = json.loads(blobs['transitions.json'])
    rows = table['transitions']
    assert table['schema_version'] == 1 and len(rows) == 7
    assert len({r['path'] for r in rows}) == 7
    sources = members(blobs['source-transitions.tar.gz'])
    assert set(sources) == {side + '/' + r['path'] for r in rows for side in ('predecessor', 'successor')}
    for row in rows:
        assert row['revision'] == 'native'
        assert row['commits'] and all(len(c) == 40 for c in row['commits'])
        for side in ('predecessor', 'successor'):
            assert digest(sources[side + '/' + row['path']]) == row[side + '_sha256']
    old = members(blobs['predecessor-ledgers.tar.gz'])
    # Preserve every original field/candidate row and every original family seal.
    for family in ('xcf', 'mypaint', 'config'):
        path = 'migration/inventory/persistence-' + family + '-fields.json'
        assert (root / path).read_bytes() == old[path], ('predecessor family changed', family)
    return table


def resolve(root, revision, path, predecessor, actual):
    """Return provenance only for an exact reviewed predecessor/current pair."""
    if predecessor == actual:
        return None
    rows = reviewed(root)['transitions']
    row = next((r for r in rows if (r['revision'], r['path']) == (revision, path)), None)
    assert row is not None, ('unreviewed persistence source drift', revision, path)
    assert predecessor == row['predecessor_sha256'], ('wrong persistence predecessor', path)
    assert actual == row['successor_sha256'], ('unreviewed persistence successor', path)
    return row.copy()

"""Exact append-only native Undo enum admission; no persisted schema change."""
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
PACKAGE = 'migration/tests/persistence-undo-successor'
PATH = 'app/core/core-enums.h'
PINS = {'transition.json': 'fd4d2006df3c53ccfa66c13c9024319cb3570dd8ba6bbaadab5260e51de71aa2', 'sources.tar.gz': '344456f40253f1d3c51c638c26f776c55d1306471e699ce66c097f060cc73efe'}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def enum(data):
    m = re.search(rb'typedef enum /\*< pdb-skip >\*/\s*\{[^{}]*\} GimpUndoType;', data)
    assert m, 'Undo enum no longer PDB-skipped'
    body = re.sub(rb'/\*.*?\*/', b'', m[0], flags=re.S).split(b'{', 1)[1].split(b'}')[0]
    values = {}; value = -1
    for entry in body.split(b','):
        if not entry.strip(): continue
        parts = entry.decode().split('='); name = parts[0].strip()
        assert re.fullmatch(r'GIMP_UNDO_[A-Z_]+', name) and name not in values
        assert len(parts) <= 2
        if len(parts) == 2:
            expr = parts[1].strip()
            value = int(expr) if expr.isdigit() else values[expr]
        else: value += 1
        values[name] = value
    return m, values

def exact_change(before, after):
    # Preserve every old description/annotation as well as numeric values.
    appendix = b'\n  /* Appended Painter extensions preserve all existing numeric values.\n   * MYPAINT group is outside the legacy contiguous group range. */\n  GIMP_UNDO_GROUP_MYPAINT = 110,         /*< desc="Mypaint"                       >*/\n  GIMP_UNDO_MYPAINT = 111                /*< desc="Mypaint"                       >*/\n'
    expected = before.replace(b'GIMP_UNDO_CANT ', b'GIMP_UNDO_CANT,', 1).replace(b'} GimpUndoType;', appendix + b'} GimpUndoType;', 1)
    assert after == expected, 'not the exact append-only Undo header transition'
    old, a = enum(before); new, b = enum(after)
    assert len(a) == 112 and len(b) == 114, 'Undo entry count changed'
    assert all(b.get(k) == v for k, v in a.items()), 'old Undo numeric value changed'
    assert {k:v for k,v in b.items() if k not in a} == {'GIMP_UNDO_GROUP_MYPAINT':110, 'GIMP_UNDO_MYPAINT':111}, 'new Undo IDs differ'
    assert (b['GIMP_UNDO_GROUP_FIRST'], b['GIMP_UNDO_GROUP_LAST']) == (0,48), 'old group range changed'
    assert before[:old.start()] + before[old.end():] == after[:new.start()] + after[new.end():], 'unrelated header bytes changed'

def reviewed(root):
    root = Path(root)
    blobs = {n:(root/PACKAGE/n).read_bytes() for n in PINS}
    for n, data in blobs.items(): assert sha(data) == PINS[n], ('Undo successor proof changed', n)
    record = json.loads(blobs['transition.json'])
    assert record['path'] == PATH and record['revision'] == 'native'
    with tarfile.open(fileobj=io.BytesIO(blobs['sources.tar.gz']), mode='r:gz') as tar:
        assert tar.getnames() == ['predecessor.h','successor.h'] and all(m.isfile() for m in tar.getmembers())
        before = tar.extractfile('predecessor.h').read(); after = tar.extractfile('successor.h').read()
    assert sha(before) == record['predecessor_sha256'] and sha(after) == record['successor_sha256']
    exact_change(before, after)
    assert (root/PATH).read_bytes() == after, 'unreviewed Undo header drift'
    return record

def resolve_source(root, predecessor, actual):
    record = reviewed(root)
    assert predecessor == record['predecessor_sha256'], 'wrong Undo predecessor'
    assert actual == record['successor_sha256'], 'wrong Undo successor'
    return record.copy()

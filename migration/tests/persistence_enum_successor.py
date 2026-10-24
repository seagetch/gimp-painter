"""Explicit public-registrar successor for the unchanged saved brush-mode enum.

Original family fields, seals and evidence remain immutable. Relocated evidence
is disclosed separately; no unknown metadata/source drift is tolerated.
"""
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
from persistence_source import function, norm

PACKAGE = 'migration/tests/persistence-enum-successor'
OPTIONS = 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'
HEADER = 'libgimpbase/gimpbaseenums.h'
REGISTRAR = 'libgimpbase/gimpbaseenums.c'
PINS = {
    'transition.json': '6cbbedb999f0ac21ead9681b5587ef9d47387baf4446d992cfc7bf25bf63fbcf',
    'sources.tar.gz': 'fab281ca702a52461175f351ac7be61899fb10d230d7b5817c9e17121168ff65',
}
VALUES = [(0, 'GIMP_MYPAINT_NORMAL', 'normal'),
          (1, 'GIMP_MYPAINT_NORMAL_AND_ERASE', 'normal-and-erase'),
          (2, 'GIMP_MYPAINT_LOCK_ALPHA', 'lock-alpha'),
          (3, 'GIMP_MYPAINT_COLORIZE', 'colorize')]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def semantic_proof(prior, current):
    """Independent wire tuple/body comparison, not equality of ledger rows."""
    old = prior[OPTIONS]
    new = current[OPTIONS]
    old_values = [(int(n), name, nick) for n, name, nick in re.findall(
        r'\{([0-3]), "(GIMP_MYPAINT_[A-Z_]+)", "([a-z-]+)"\}',
        function(old, 'legacy_brush_mode_type'))]
    numeric = dict((name, int(n)) for name, n in re.findall(
        r'(GIMP_MYPAINT_[A-Z_]+)\s*=\s*(-?\d+)\b', current[HEADER]))
    registrar = function(current[REGISTRAR], 'gimp_mypaint_brush_mode_get_type')
    new_values = [(numeric[symbol], name, nick) for symbol, name, nick in re.findall(
        r'\{\s*(GIMP_MYPAINT_[A-Z_]+), "(GIMP_MYPAINT_[A-Z_]+)", "([a-z-]+)"\s*\}', registrar)]
    assert old_values == new_values == VALUES, 'persisted enum tuple changed'
    assert numeric == {name: number for number, name, nick in VALUES}, 'enum integer domain changed'
    for symbol, name, nick in re.findall(
            r'\{\s*(GIMP_MYPAINT_[A-Z_]+), "(GIMP_MYPAINT_[A-Z_]+)", "([a-z-]+)"\s*\}', registrar):
        assert symbol == name, 'enum symbol/name differs'
    for body in (function(old, 'legacy_brush_mode_type'), registrar):
        assert 'g_enum_register_static ("GimpMypaintBrushMode", values)' in body
    assert '#define GIMP_TYPE_MYPAINT_BRUSH_MODE (gimp_mypaint_brush_mode_get_type ())' in current[HEADER]
    for name in ('set_property', 'get_property', 'gimp_painter_mybrush_options_class_init'):
        before = norm(function(old, name))
        after = norm(function(new, name))
        if name.endswith('class_init'):
            assert before.count('legacy_brush_mode_type()') == 1
            before = before.replace('legacy_brush_mode_type()', 'GIMP_TYPE_MYPAINT_BRUSH_MODE')
        assert before == after, ('saved property implementation changed', name)
    assert '\tgimp_mypaint_brush_mode_get_type\n' in current['libgimpbase/gimpbase.def']
    assert 'gimp_mypaint_brush_mode_get_type,' in current['libgimp/gimpenums.c.tail']
    assert '"GimpMypaintBrushMode",' in current['libgimp/gimpenums.c.tail']
    assert 'GimpMypaintBrushMode =>' in current['pdb/enums.pl']
    assert '} GimpMypaintBrushMode;' not in current['app/paint/gimpmypaintoptions.h']
    return new_values


def reviewed(root):
    root = Path(root)
    blobs = {name: (root / PACKAGE / name).read_bytes() for name in PINS}
    for name, data in blobs.items():
        assert sha(data) == PINS[name], ('enum successor proof changed', name)
    data = json.loads(blobs['transition.json'])
    with tarfile.open(fileobj=io.BytesIO(blobs['sources.tar.gz']), mode='r:gz') as archive:
        names = archive.getnames()
        assert len(set(names)) == len(names) == 14
        assert all(m.isfile() for m in archive.getmembers())
        archived = {name: archive.extractfile(name).read() for name in names}
    assert len(data['providers']) == len({r['path'] for r in data['providers']}) == 7
    assert set(archived) == {side + '/' + r['path'] for r in data['providers'] for side in ('predecessor', 'successor')}
    before = {}; after = {}
    for row in data['providers']:
        path = row['path']
        for side in ('predecessor', 'successor'):
            assert sha(archived[side + '/' + path]) == row[side + '_sha256']
        current = (root / path).read_bytes()
        if sha(current) != row['successor_sha256']:
            from persistence_readiness_successor import provider
            try:
                provider(root, path, row['successor_sha256'], sha(current))
            except (AssertionError, FileNotFoundError) as error:
                raise AssertionError(('enum provider drift', path)) from error
        before[path] = archived['predecessor/' + path].decode()
        if path == OPTIONS and b'g_param_spec_boolean(s.internal_name,s.displayed_name,nullptr,s.default_value,flags)' in current:
            from persistence_switch_default_successor import property_predecessor
            after[path] = property_predecessor(root,path,current).decode()
        else:
            after[path] = current.decode()
    semantic_proof(before, after)
    return data


def mode_values(root, options):
    if 'GType legacy_brush_mode_type ()' in options:
        return re.findall(r'\{([0-3]), "(GIMP_MYPAINT_[A-Z_]+)", "([a-z-]+)"\}', function(options, 'legacy_brush_mode_type'))
    reviewed(root)
    return [(str(n), name, nick) for n, name, nick in VALUES]


def resolve_source(root, previous, actual):
    data = reviewed(root)
    row = next(r for r in data['providers'] if r['path'] == OPTIONS)
    assert previous['path'] == OPTIONS and previous['revision'] == 'native'
    assert previous['successor_sha256'] == row['predecessor_sha256'], 'enum chain predecessor mismatch'
    result = dict(previous, successor_sha256=row['successor_sha256'],
                intermediate_sha256=previous['successor_sha256'],
                commits=previous['commits'] + [data['successor_commit']],
                semantic_successor='public-enum-registrar; unchanged saved values/default/flags')
    if actual != row['successor_sha256']:
        from persistence_readiness_successor import resolve_source as readiness_source
        return readiness_source(root, OPTIONS, row['successor_sha256'], actual, result)
    return result


def resolve_evidence(root, field_id, evidence, options):
    if 'GType legacy_brush_mode_type ()' in options or field_id != 'mypaint.option.mode':
        return evidence
    for row in reviewed(root)['evidence_successors']:
        if row['field_id'] == field_id and row['predecessor'] == evidence:
            return row['successor'].copy()
    return evidence


def provenance(root):
    options = (Path(root) / OPTIONS).read_text()
    return None if 'GType legacy_brush_mode_type ()' in options else reviewed(root)

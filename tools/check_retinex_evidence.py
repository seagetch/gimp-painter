#!/usr/bin/env python3
"""Verify or materialize the sealed exact-byte Retinex-only evidence.

This verifies historical evidence, not the current implementation. Run the
native/helper/capture tests separately to establish current behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

from derive_retinex_evidence import SOURCE_HASHES, SOURCE_MANIFEST_HASHES

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARCHIVE = ROOT / 'migration/fixtures/retinex-evidence.tar.gz'
PREFIX = 'retinex-evidence/'
PINNED_ARCHIVE = '28f1f803e72bb25cb7fedcc4cf108a3df71a492d458baa1cbf8f52438b8e946c'
PINNED_MANIFEST = '6542f4503c344a907611840011039c7957e208b6201fe46dc891830f2f6825cf'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def verify(archive=DEFAULT_ARCHIVE, extract=None):
    archive = Path(archive)
    raw = archive.read_bytes()
    manifest_raw = archive.with_suffix('.manifest.json').read_bytes()
    if sha(manifest_raw) != PINNED_MANIFEST:
        raise ValueError('Unrecognized or changed Retinex member manifest')
    manifest = json.loads(manifest_raw)
    if sha(raw) != PINNED_ARCHIVE or manifest['archive_sha256'] != PINNED_ARCHIVE:
        raise ValueError('Unrecognized or changed Retinex archive')
    if manifest['kind'] != 'derived-retinex-evidence' or manifest['schema_version'] != 1:
        raise ValueError('Unrecognized Retinex manifest')
    if {k:v['archive_sha256'] for k,v in manifest['sources'].items()} != SOURCE_HASHES:
        raise ValueError('Original archive linkage changed')
    if {k:v['manifest_sha256'] for k,v in manifest['sources'].items()} != SOURCE_MANIFEST_HASHES:
        raise ValueError('Original member-manifest linkage changed')
    expected_counts = dict(old_pdb=174, public_before=32, public_after=32,
                           hidden_offtree=174, hidden_integrated=174,
                           native_scenes=6, native_merges=12)
    if manifest['counts'] != expected_counts:
        raise ValueError('Wrong Retinex subset counts')
    members = manifest['members']
    if len(members) != manifest['member_count'] or len(members) > 1500 or manifest['total_bytes'] > 16*1024*1024:
        raise ValueError('Unbounded evidence manifest')
    payload = {}
    with tarfile.open(archive, 'r:gz') as tar:
        for member in tar:
            path = PurePosixPath(member.name)
            if (not member.isfile() or member.name in payload or member.name not in members or
                not member.name.startswith(PREFIX) or path.is_absolute() or
                any(p in ('', '.', '..') for p in member.name.split('/')) or member.size > 1024*1024):
                raise ValueError('Unsafe or duplicate evidence member')
            data = tar.extractfile(member).read(member.size+1)
            item = members[member.name]
            if len(data) != member.size or len(data) != item['size'] or sha(data) != item['sha256']:
                raise ValueError('Changed evidence member: ' + member.name)
            origin = item.get('derived_from', item)
            source = origin.get('source_archive')
            if source not in [n+'.tar.gz' for n in SOURCE_HASHES]:
                raise ValueError('Missing original archive/member linkage')
            if not origin.get('source_member') or not origin.get('source_sha256'):
                raise ValueError('Missing original member hash')
            if 'derived_from' not in item and (item['source_sha256'] != item['sha256'] or
                                               item['source_size'] != item['size']):
                raise ValueError('Copied evidence differs from original member')
            payload[member.name] = data
    if set(payload) != set(members) or sum(map(len, payload.values())) != manifest['total_bytes']:
        raise ValueError('Incomplete Retinex archive')
    def data(path):
        return payload[PREFIX+path]
    def document(path):
        return json.loads(data(path))
    old_report = document('provenance/legacy-additional-filters-capture-report.json')
    if (old_report['status'] != 'passed' or old_report['errors'] or old_report['case_count'] != 370 or
        old_report['source_commit'] != 'afa43fae3e920210146abed514f136fd49f671b5'):
        raise ValueError('Original legacy provenance changed')
    group_counts = {'pdb':174, 'public-before':32, 'public-after':32, 'hidden-offtree':174, 'hidden-integrated':174}
    old_roots = {'pdb':'legacy-additional-filters', 'public-before':'modern-additional-before',
                 'public-after':'modern-additional-after', 'hidden-offtree':'hidden-additional-offtree',
                 'hidden-integrated':'hidden-additional-integrated'}
    indexes = {}
    for group, count in group_counts.items():
        index = document(group+'/index.json')
        original_report = old_roots[group]+'-capture-report.json'
        if (index['schema_version'] != 1 or index['kind'] != 'derived-retinex-index' or
            index['original_report'] != '../provenance/'+original_report):
            raise ValueError('Invalid derived index provenance: '+group)
        report = document('provenance/'+original_report)
        if report['status'] != 'passed' or report['errors']:
            raise ValueError('Unpassed historical capture: '+group)
        selected = [c for c in report['cases'] if c['procedure'].endswith('retinex')]
        if index['cases'] != selected or len(selected) != count or index['case_count'] != count:
            raise ValueError('Derived cases do not match original provenance: '+group)
        indexes[group] = {c['id']:c for c in selected}
        if len(indexes[group]) != count:
            raise ValueError('Duplicate Retinex case ID: '+group)
        for case in selected:
            expected_size = case['width']*case['height']*case['channels']
            if len(data(group+'/'+case['input'])) != expected_size or len(data(group+'/'+case['output'])) != expected_size:
                raise ValueError('Wrong raster extent: '+case['id'])
            if sha(data(group+'/'+case['output'])) != case['output_sha256']:
                raise ValueError('Raster differs from original report: '+case['id'])
        if group != 'pdb':
            signatures = document('provenance/'+old_roots[group]+'-signatures.json')
            selected_signatures = {key:value for key,value in signatures.items() if key.endswith('retinex')}
            if document(group+'/signatures.json') != selected_signatures:
                raise ValueError('Derived signatures differ from original provenance: '+group)
    rows = ['\t'.join(map(str, [case['procedure'], case['width'], case['height'], case['channels'],
                              *case['parameters'], case['input'], case['output']]))
            for case in indexes['pdb'].values()]
    if data('pdb/fixtures.tsv') != ('\n'.join(rows)+'\n').encode():
        raise ValueError('Derived Retinex helper fixture index changed')
    if set(indexes['public-before']) != set(indexes['public-after']):
        raise ValueError('Public cases changed')
    for ident, before in indexes['public-before'].items():
        after = indexes['public-after'][ident]
        if data('public-before/'+before['output']) != data('public-after/'+after['output']):
            raise ValueError('Historic public Retinex output changed')
    if document('public-before/signatures.json') != document('public-after/signatures.json'):
        raise ValueError('Historic public Retinex signature changed')
    for group in ('hidden-offtree', 'hidden-integrated'):
        if set(indexes[group]) != set(indexes['pdb']):
            raise ValueError('Historic hidden case set differs')
        for ident, case in indexes[group].items():
            output = bytearray(data(group+'/'+case['output']))
            original = data(group+'/'+case['input'])
            channels = case['channels']
            if case['mode'].endswith('A'):
                for at in range(0, len(output), channels):
                    if output[at+channels-1] == 0:
                        output[at:at+channels-1] = original[at:at+channels-1]
            if bytes(output) != data('pdb/'+indexes['pdb'][ident]['output']):
                raise ValueError('Historic hidden Retinex differs after old zero-alpha merge')
    live = document('provenance/legacy-filter-additional-live-capture-report.json')
    if (live['status'] != 'passed' or live['errors'] or live['scene_count'] != 31 or
        live['live_merge_count'] != 60 or live['native_merge_count'] != 0 or
        live['source_commit'] != old_report['source_commit']):
        raise ValueError('Original native scene provenance changed')
    for filename, count in (('cases.tsv', 6), ('merges.tsv', 12), ('bounds.tsv', 12), ('events.tsv', 30)):
        original = data('provenance/legacy-filter-additional-live-'+filename)
        if sha(original) != live['files_sha256'][filename]:
            raise ValueError('Original native table provenance changed: '+filename)
        selected = b''.join(row for row in original.splitlines(keepends=True) if row.startswith(b'retinex-'))
        if data('live/'+filename) != selected or len(selected.splitlines()) != count:
            raise ValueError('Native Retinex rows differ from original provenance: '+filename)
    cases = [row.split('\t') for row in data('live/cases.tsv').decode().splitlines()]
    expected_ids = {'retinex-g0-s%d-v%d' % (selection, variant)
                    for selection in range(3) for variant in range(2)}
    if {row[0] for row in cases} != expected_ids or any(len(row) != 10 for row in cases):
        raise ValueError('Wrong Retinex native scene identifiers or arguments')
    expected_live_raw = set()
    for row in cases:
        ident, procedure, width, height, channels, factor, scale, nscales, mode, dynamic = row
        if procedure != 'plug-in-retinex' or factor != '-1':
            raise ValueError('Wrong native Retinex procedure or argument sentinel')
        extent = int(width)*int(height)*int(channels)
        for suffix in ('source', 'settled', 'rerun', 'start1', 'start2'):
            filename = ident+'-'+suffix+'.raw'
            expected_live_raw.add(PREFIX+'live/'+filename)
            if len(data('live/'+filename)) != extent:
                raise ValueError('Wrong native Retinex scene raster extent: '+filename)
        for merge in (1, 2):
            for suffix in ('input', 'output', 'shadow', 'mask'):
                filename = ident+'-merge%d-' % merge+suffix+'.raw'
                expected_live_raw.add(PREFIX+'live/'+filename)
                size = int(width)*int(height) if suffix == 'mask' else extent
                if len(data('live/'+filename)) != size:
                    raise ValueError('Wrong native Retinex merge raster extent: '+filename)
    actual_live_raw = {name for name in payload if name.startswith(PREFIX+'live/') and name.endswith('.raw')}
    if actual_live_raw != expected_live_raw:
        raise ValueError('Incomplete or unexpected native Retinex rasters')
    for name, blob in payload.items():
        if name.startswith(PREFIX+'live/') and name.endswith('.raw'):
            original = name.split('/')[-1]
            if sha(blob) != live['files_sha256'][original]:
                raise ValueError('Live raster disagrees with original provenance')
    if extract is not None:
        destination = Path(extract)
        destination.mkdir(parents=True, exist_ok=True)
        if any(destination.iterdir()):
            raise ValueError('Extraction requires an empty isolated directory')
        for name, blob in payload.items():
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('xb') as stream:
                stream.write(blob)
    return manifest

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument('--extract', type=Path)
    args = parser.parse_args()
    manifest = verify(args.archive, args.extract)
    print(json.dumps(dict(status='passed', historical_only=True, counts=manifest['counts'],
                          archive_sha256=manifest['archive_sha256'])))

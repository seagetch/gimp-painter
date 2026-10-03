#!/usr/bin/env python3
"""Verify or materialize the sealed exact-byte SmallTiles-only evidence.

This verifies historical evidence, not the current implementation. Run the
native/helper/capture tests separately to establish current behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

from derive_small_tiles_evidence import SOURCE_HASHES

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARCHIVE = ROOT / 'migration/fixtures/small-tiles-evidence.tar.gz'
PREFIX = 'small-tiles-evidence/'
PINNED_ARCHIVE = 'd28e4b74436f980a9e263225748b056df2228ed6b5d8b815f4b6e1c1d67f8695'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def verify(archive=DEFAULT_ARCHIVE, extract=None):
    archive = Path(archive)
    raw = archive.read_bytes()
    manifest = json.loads(archive.with_suffix('.manifest.json').read_text())
    if sha(raw) != PINNED_ARCHIVE or manifest['archive_sha256'] != PINNED_ARCHIVE:
        raise ValueError('Unrecognized or changed SmallTiles archive')
    if manifest['kind'] != 'derived-small-tiles-evidence' or manifest['schema_version'] != 1:
        raise ValueError('Unrecognized SmallTiles manifest')
    if {k:v['archive_sha256'] for k,v in manifest['sources'].items()} != SOURCE_HASHES:
        raise ValueError('Original archive linkage changed')
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
            source = item.get('source_archive', item.get('derived_from', {}).get('source_archive'))
            if source not in [n+'.tar.gz' for n in SOURCE_HASHES]:
                raise ValueError('Missing original archive/member linkage')
            payload[member.name] = data
    if set(payload) != set(members) or sum(map(len, payload.values())) != manifest['total_bytes']:
        raise ValueError('Incomplete SmallTiles archive')
    def data(path):
        return payload[PREFIX+path]
    def document(path):
        return json.loads(data(path))
    old_report = document('provenance/legacy-additional-filters-capture-report.json')
    if (old_report['status'] != 'passed' or old_report['errors'] or old_report['case_count'] != 370 or
        old_report['source_commit'] != 'afa43fae3e920210146abed514f136fd49f671b5'):
        raise ValueError('Original legacy provenance changed')
    group_counts = {'pdb':196, 'public-before':24, 'public-after':24, 'hidden-offtree':196, 'hidden-integrated':196}
    indexes = {}
    for group, count in group_counts.items():
        index = document(group+'/index.json')
        report = document('provenance/'+index['original_report'].split('/')[-1])
        selected = [c for c in report['cases'] if c['procedure'].endswith('small-tiles')]
        if index['cases'] != selected or len(selected) != count or index['case_count'] != count:
            raise ValueError('Derived cases do not match original provenance: '+group)
        indexes[group] = {c['id']:c for c in selected}
        for case in selected:
            expected_size = case['width']*case['height']*case['channels']
            if len(data(group+'/'+case['input'])) != expected_size or len(data(group+'/'+case['output'])) != expected_size:
                raise ValueError('Wrong raster extent: '+case['id'])
            if sha(data(group+'/'+case['output'])) != case['output_sha256']:
                raise ValueError('Raster differs from original report: '+case['id'])
    if set(indexes['public-before']) != set(indexes['public-after']):
        raise ValueError('Public cases changed')
    for ident, before in indexes['public-before'].items():
        after = indexes['public-after'][ident]
        if data('public-before/'+before['output']) != data('public-after/'+after['output']):
            raise ValueError('Historic public SmallTiles output changed')
    if document('public-before/signatures.json') != document('public-after/signatures.json'):
        raise ValueError('Historic public SmallTiles signature changed')
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
                raise ValueError('Historic hidden SmallTiles differs after old zero-alpha merge')
    live = document('provenance/legacy-filter-additional-live-capture-report.json')
    if live['status'] != 'passed' or live['errors'] or live['scene_count'] != 31 or live['live_merge_count'] != 60:
        raise ValueError('Original native scene provenance changed')
    cases = data('live/cases.tsv').decode().splitlines()
    merges = data('live/merges.tsv').decode().splitlines()
    if len(cases) != 25 or len(merges) != 48:
        raise ValueError('Wrong SmallTiles native subset counts')
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

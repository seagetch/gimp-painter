#!/usr/bin/env python3
"""Verify or safely extract the sealed genuine old convolution observations.

Checks evidence integrity only; implementation parity needs separate tests.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'migration/fixtures/legacy-convolution.tar.gz'
ARCHIVE_SHA256 = 'dff2d7f6b4b6d514e0c213abd7d2757729708a10b821b12316b9c97102f84a57'
MANIFEST_SHA256 = '27a3ffb3feed3b14e110915c0bab4a5a6412946a09561939ebbe12b5d5c2b44f'
PREFIX = 'legacy-convolution/'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify(archive=DEFAULT, extract=None):
    archive = Path(archive)
    raw = archive.read_bytes()
    manifest_raw = archive.with_suffix('.manifest.json').read_bytes()
    if len(raw) > 2 * 1024 * 1024 or sha(raw) != ARCHIVE_SHA256 or sha(manifest_raw) != MANIFEST_SHA256:
        raise ValueError('Changed or unbounded convolution evidence archive/manifest')
    manifest = json.loads(manifest_raw)
    if (manifest['schema_version'] != 1 or manifest['archive_sha256'] != ARCHIVE_SHA256 or
        manifest['roots'] != ['legacy-convolution'] or manifest['member_count'] != len(manifest['members']) or
        manifest['member_count'] > 100 or manifest['total_bytes'] > 4 * 1024 * 1024):
        raise ValueError('Invalid convolution evidence manifest')
    payload = {}
    with tarfile.open(archive, 'r:gz') as tar:
        for member in tar:
            path = PurePosixPath(member.name)
            if (not member.isfile() or member.name in payload or member.name not in manifest['members'] or
                not member.name.startswith(PREFIX) or path.is_absolute() or
                any(p in ('', '.', '..') for p in member.name.split('/')) or member.size > 1024 * 1024):
                raise ValueError('Unsafe or duplicate convolution archive member')
            data = tar.extractfile(member).read(member.size + 1)
            expected = manifest['members'][member.name]
            if len(data) != member.size or len(data) != expected['size'] or sha(data) != expected['sha256']:
                raise ValueError('Changed convolution archive member ' + member.name)
            payload[member.name] = data
    if set(payload) != set(manifest['members']) or sum(map(len,payload.values())) != manifest['total_bytes']:
        raise ValueError('Incomplete convolution evidence archive')
    def document(name): return json.loads(payload[PREFIX + name])
    report = document('capture-report.json')
    if (report['status'] != 'passed' or report['errors'] or report['case_count'] != 304 or
        report['source_commit'] != 'afa43fae3e920210146abed514f136fd49f671b5'):
        raise ValueError('Invalid old capture provenance')
    for name, expected in report['files_sha256'].items():
        if sha(payload[PREFIX + name]) != expected:
            raise ValueError('Old capture artifact changed ' + name)
    fixture = document('fixtures.json'); pixels = payload[PREFIX + fixture['pixel_file']]
    if sha(pixels) != fixture['pixel_sha256'] or len(fixture['cases']) != 304:
        raise ValueError('Invalid old convolution fixture index')
    ids = set(); successes = rejected = live = 0
    for case in fixture['cases']:
        if case['id'] in ids: raise ValueError('Duplicate old case')
        ids.add(case['id'])
        if (len(case['matrix']) != 25 or len(case['channels']) != 5 or
            not 1 <= case['components'] <= 4 or not 1 <= case['output_components'] <= 4):
            raise ValueError('Invalid old case parameters')
        expected = 3 if min(case['width'],case['height']) >= 3 else 0
        if case['status'] != expected: raise ValueError('Wrong old case status')
        successes += expected == 3; rejected += expected == 0; live += bool(case['live'])
        if expected == 3 and case['merges'] != 1: raise ValueError('Missing actual shadow merge')
        if case['live'] and (case['starts'],case['ends']) != (1,1): raise ValueError('Missing real FilterLayer execution')
        for name, buffer in case['buffers'].items():
            offset,length = buffer['offset'],buffer['length']
            if offset < 0 or length < 0 or offset + length > len(pixels): raise ValueError('Invalid old byte extent')
            if sha(pixels[offset:offset+length]) != buffer['sha256']: raise ValueError('Changed old byte buffer')
            components = 1 if name.endswith('-mask') else case['components'] if name == 'source' else case['output_components']
            if length != case['width'] * case['height'] * components: raise ValueError('Invalid old raster dimensions')
    if (successes,rejected,live) != (296,8,12): raise ValueError('Changed old case coverage')
    repeat = document('repeat-report.json')
    if repeat['exit_code'] or repeat['matched'] != 304 or repeat['failures']: raise ValueError('Invalid independent repeat')
    if extract is not None:
        destination = Path(extract)
        if destination.is_symlink() or (destination.exists() and any(destination.iterdir())):
            raise ValueError('Extraction destination must be absent or empty and not a symlink')
        destination.mkdir(parents=True,exist_ok=True)
        destination = destination.resolve()
        for name,data in payload.items():
            target = destination / name
            target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as stream: stream.write(data)
        # Consumer-only derivative; the immutable archive and JSON stay unchanged.
        rows = []
        buffer_names = ('input', 'source', 'merge1-input', 'merge1-shadow',
                        'merge1-mask', 'merge1-output', 'output')
        for case in fixture['cases']:
            fields = [case['id'], case['width'], case['height'], case['output_components'],
                      case['status'], case['live'], *case['initial_roi'],
                      case['alpha_weight'], repr(case['divisor']), repr(case['offset']),
                      case['border'], *case['channels'],
                      *(repr(value) for value in case['matrix'])]
            assert len(fields) == 45
            for name in buffer_names:
                buffer = case['buffers'].get(name, {'offset': 0, 'length': 0})
                fields.extend((buffer['offset'], buffer['length']))
            rows.append('\t'.join(map(str, fields)))
        table = destination / PREFIX / 'cases.tsv'
        with table.open('x') as stream:
            stream.write('\n'.join(rows) + '\n')
    return dict(archive_sha256=ARCHIVE_SHA256,case_count=304,successful_calls=296,
                expected_tiny_rejections=8,actual_filter_layer_calls=12,repeat_matches=304,
                negative_diagnostic_cases=12)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,default=DEFAULT)
    parser.add_argument('--extract',type=Path)
    args=parser.parse_args()
    print(json.dumps(verify(args.archive,args.extract),sort_keys=True))

if __name__ == '__main__': main()

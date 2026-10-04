#!/usr/bin/env python3
"""Derive the sealed Retinex subset from immutable earlier mixed captures.

No image processing or recapture occurs here. Original report bytes are retained
under provenance; derived indexes are explicitly labelled, never old reports.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_HASHES = {
    'legacy-additional-filters': 'e3e3b804e76693ddfd27da381a0e2bab92eba7b6fe1b5d5f44c8ea62dbf076ae',
    'additional-filter-plugin-evidence': '59445d1797a8d36b02575e3f8458aea0e886d44b86fbed56ac5a88a9df9c3bf1',
    'legacy-filter-additional-live': '6fce7cff0b6d19bd3867ac305c3844f67c72ee41ed18525390ab48985352db47',
}
SOURCE_MANIFEST_HASHES = {
    'legacy-additional-filters': '2dd2538ea1cb4213053d36546fd95378bcf9d29cddae63efff4615d8bd221789',
    'additional-filter-plugin-evidence': '1ae94290c7865c81c29745afb53bd36b8aca1b3b7e645b7ec3a1d72643ededb5',
    'legacy-filter-additional-live': '8befeddacea9556e819a456d698945568f7a5dd5915686937a90b00a77e78f31',
}
PREFIX = 'retinex-evidence/'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def derive(source, output):
    archives = {}
    original_manifests = {}
    sources = {}
    for name, expected in SOURCE_HASHES.items():
        path = source / (name + '.tar.gz')
        data = path.read_bytes()
        if sha(data) != expected:
            raise ValueError('Original archive changed: ' + str(path))
        manifest_path = path.with_suffix('.manifest.json')
        manifest_data = manifest_path.read_bytes()
        if sha(manifest_data) != SOURCE_MANIFEST_HASHES[name]:
            raise ValueError('Original member manifest changed: ' + str(manifest_path))
        manifest = json.loads(manifest_data)
        if manifest['archive_sha256'] != expected:
            raise ValueError('Original manifest disagrees: ' + str(path))
        archives[name] = tarfile.open(fileobj=io.BytesIO(data), mode='r:gz')
        original_manifests[name] = manifest
        sources[name] = dict(archive=path.name, archive_sha256=expected,
                             manifest_sha256=sha(manifest_data))
    members, payload = {}, {}
    def read(name, member):
        data = archives[name].extractfile(member).read()
        original = original_manifests[name]['members'][member]
        if len(data) != original['size'] or sha(data) != original['sha256']:
            raise ValueError('Original member changed: ' + member)
        return data
    def add(path, data, origin):
        path = PREFIX + path
        if path in payload:
            if data != payload[path]:
                raise ValueError('Conflicting destination ' + path)
            return
        payload[path] = data
        members[path] = dict(size=len(data), sha256=sha(data), **origin)
    def copy(name, member, path):
        original = read(name, member)
        add(path, original, dict(source_archive=name + '.tar.gz', source_member=member,
                                 source_sha256=sha(original), source_size=len(original)))
    def generated(path, data, name, member, rule):
        original = read(name, member)
        add(path, data, dict(derived_from=dict(source_archive=name + '.tar.gz', source_member=member,
                                              source_sha256=sha(original), source_size=len(original), rule=rule)))
    groups = [('pdb', 'legacy-additional-filters', 'legacy-additional-filters', 174),
              ('public-before', 'additional-filter-plugin-evidence', 'modern-additional-before', 32),
              ('public-after', 'additional-filter-plugin-evidence', 'modern-additional-after', 32),
              ('hidden-offtree', 'additional-filter-plugin-evidence', 'hidden-additional-offtree', 174),
              ('hidden-integrated', 'additional-filter-plugin-evidence', 'hidden-additional-integrated', 174)]
    for group, name, old_root, count in groups:
        report_member = old_root + '/capture-report.json'
        original = json.loads(read(name, report_member))
        if original['status'] != 'passed' or original['errors']:
            raise ValueError('Unpassed capture ' + old_root)
        cases = [case for case in original['cases'] if case['procedure'].endswith('retinex')]
        if len(cases) != count:
            raise ValueError('Wrong Retinex case count: ' + old_root)
        copy(name, report_member, 'provenance/' + old_root + '-capture-report.json')
        index = dict(schema_version=1, kind='derived-retinex-index', case_count=count,
                     original_report='../provenance/' + old_root + '-capture-report.json', cases=cases)
        generated(group + '/index.json', (json.dumps(index, indent=2) + '\n').encode(), name,
                  report_member, 'Retain unchanged cases whose procedure ends with retinex')
        for case in cases:
            for key in ('input', 'output'):
                copy(name, old_root + '/' + case[key], group + '/' + case[key])
            if group == 'pdb':
                png = 'input-' + case['mode'] + '-%sx%s.png' % (case['width'], case['height'])
                copy(name, old_root + '/' + png, group + '/' + png)
        if group == 'pdb':
            rows = ['\t'.join(map(str, [c['procedure'], c['width'], c['height'], c['channels'], *c['parameters'], c['input'], c['output']])) for c in cases]
            generated(group + '/fixtures.tsv', ('\n'.join(rows)+'\n').encode(), name,
                      report_member, 'Retinex rows: procedure width height channels scale nscales scale_mode dynamic input output')
        else:
            signatures = old_root + '/signatures.json'
            copy(name, signatures, 'provenance/' + old_root + '-signatures.json')
            all_signatures = json.loads(read(name, signatures))
            selected = {k:v for k,v in all_signatures.items() if k.endswith('retinex')}
            generated(group + '/signatures.json', (json.dumps(selected, indent=2)+'\n').encode(), name,
                      signatures, 'Retain Retinex procedure metadata unchanged')
    name = 'legacy-filter-additional-live'
    root = name + '/'
    copy(name, root + 'capture-report.json', 'provenance/' + name + '-capture-report.json')
    for filename in ('cases.tsv', 'merges.tsv', 'bounds.tsv', 'events.tsv'):
        data = read(name, root + filename)
        copy(name, root + filename, 'provenance/' + name + '-' + filename)
        selected = b''.join(row for row in data.splitlines(keepends=True) if row.startswith(b'retinex-'))
        generated('live/' + filename, selected, name, root + filename, 'Retain unchanged Retinex rows beginning retinex-')
    for member in archives[name].getmembers():
        if member.name.startswith(root + 'retinex-') and member.name.endswith('.raw'):
            copy(name, member.name, 'live/' + member.name[len(root):])
    for filename, count in (('cases.tsv', 6), ('merges.tsv', 12),
                            ('bounds.tsv', 12), ('events.tsv', 30)):
        if len(payload[PREFIX + 'live/' + filename].splitlines()) != count:
            raise ValueError('Wrong Retinex live subset count: ' + filename)
    for archive in archives.values():
        archive.close()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() or output.with_suffix('.manifest.json').exists():
        raise ValueError('Refuse to replace sealed evidence')
    with output.open('xb') as stream:
        with gzip.GzipFile(filename='', mode='wb', fileobj=stream, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as tar:
                for path, data in sorted(payload.items()):
                    info = tarfile.TarInfo(path); info.size = len(data); info.mode = 0o644
                    tar.addfile(info, io.BytesIO(data))
    manifest = dict(schema_version=1, kind='derived-retinex-evidence', archive=output.name,
                    archive_sha256=sha(output.read_bytes()), sources=sources, member_count=len(members),
                    total_bytes=sum(m['size'] for m in members.values()), members=members,
                    counts=dict(old_pdb=174, public_before=32, public_after=32,
                                hidden_offtree=174, hidden_integrated=174, native_scenes=6, native_merges=12),
                    derivation_tool_sha256=sha(Path(__file__).read_bytes()))
    output.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(dict(archive=str(output), bytes=output.stat().st_size, counts=manifest['counts'])))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Original migration/fixtures directory (read-only)')
    parser.add_argument('--output', type=Path, default=ROOT/'migration/fixtures/retinex-evidence.tar.gz')
    args = parser.parse_args()
    derive(args.source, args.output)

#!/usr/bin/env python3
"""Verify sealed genuine old Sharpen evidence and materialize a consumer view.

Integrity/provenance only. Current implementation parity is a separate native
acceptance test. Both corpora are immutable archives. The verified consumer view supplies
old-writer XCF files and bounded native buffers to the registered tests.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT/'migration/fixtures/legacy-sharpen-context.tar.xz'
LARGE = ROOT/'migration/fixtures/legacy-sharpen-large.tar.xz'
COMMIT = 'afa43fae3e920210146abed514f136fd49f671b5'
BASE_ARCHIVE_SHA256 = 'd54b50fe358aa5915303f4ea92deb26fec6df90113d7fe4b0b06b2706e5120b5'
BASE_MANIFEST_SHA256 = '269175d2a3e9e00e437c46afa1b1f2e68f60817dba8c0cacc04e11f455be6df5'
BASE_REPORT_SHA256 = 'b49915301306d9dd556335829bc07783568b7edf535eb1b5a85bebd185789a16'
LARGE_ARCHIVE_SHA256 = 'afd9483df4cff23c4ffb80d594f9468cb8e76cfe20cf04fda38e43f2190d2acd'
# Filled from the separately sealed capture, never from the implementation.
LARGE_MANIFEST_SHA256 = '37c55a760d8c30c2334a6722fa734b6bea590780f12bd40b0c8958bee0b75f15'
LARGE_REPORT_SHA256 = '65e51cc6731d3b2dc59697498e25d6c80733d8050af6dfdb4dd8fafd014a71da'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def safe_name(name):
    return bool(name) and name == Path(name).name and name not in ('.', '..') and '\\' not in name


def check_capture(files, report_hash, count, live, writers):
    report_raw = files['capture-report.json']
    if sha(report_raw) != report_hash: raise ValueError('Changed sealed Sharpen capture report')
    report = json.loads(report_raw)
    if (report['status'] != 'passed' or report['errors'] or report['exit_code'] != 0 or
        report['source_commit'] != COMMIT or report['case_count'] != count or report['expected_cases'] != count or
        report['source_sha256'] != report['post_capture_source_sha256'] or
        report['executable_sha256'] != report['post_capture_executable_sha256']):
        raise ValueError('Invalid Sharpen capture provenance/counts')
    if set(files) != set(report['files_sha256']) | {'capture-report.json'}:
        raise ValueError('Unrecorded or missing Sharpen evidence artifact')
    for name, expected in report['files_sha256'].items():
        if not safe_name(name) or sha(files[name]) != expected: raise ValueError('Changed Sharpen artifact '+name)
    for name,key in (('capture-script.py','capture_script_sha256'),('capture-harness.c','harness_sha256'),
                     ('observation-hooks.patch','instrumentation_sha256'),('observation-helper.py','observation_helper_sha256')):
        if sha(files[name]) != report[key]: raise ValueError('Changed Sharpen capture source '+name)
    fixture = json.loads(files['fixtures.json']); cases = fixture['cases']
    rows = [row.split('\t') for row in files['captures.tsv'].decode().splitlines()]
    if len(cases) != count or len(rows) != count or any(len(row) != 24 for row in rows):
        raise ValueError('Incomplete Sharpen case table')
    ids = {case['id'] for case in cases}
    if len(ids) != count or ids != {row[0] for row in rows}: raise ValueError('Duplicate/missing Sharpen IDs')
    if sum(bool(case['live']) for case in cases) != live: raise ValueError('Wrong genuine FilterLayer coverage')
    observed = {row[0]:dict(zip(fixture['capture_columns'], [row[0]]+list(map(int,row[1:])))) for row in rows}
    for case in cases:
        row = observed[case['id']]
        if row != case['observed']: raise ValueError('Sharpen manifest/table disagree')
        runs = (1+case['rerun']) if case['live'] else 0
        merges = 0 if case['selection']==3 else 1+case['rerun']
        if (row['starts'],row['ends'],row['merges'],row['status']) != (runs,runs,merges,-1 if case['live'] else 3):
            raise ValueError('Missing observed Sharpen execution/merge evidence')
        if case['width'] < 2 or case['height'] < 3 or case['percent'] < 100-2147483647:
            raise ValueError('Undefined legacy Sharpen domain captured')
        channels = (2 if case['components']<=2 else 4) if case['live'] else case['components']
        if row['output_channels'] != channels: raise ValueError('Invented no-alpha FilterLayer layout')
        required = {'source','input','output','start-mask','final-mask'}
        if case['live']: required |= {'start1','start1-mask'}
        if case['rerun']: required |= {'source2','output2','start2','start2-mask'}
        for n in range(1,merges+1): required |= {f'merge{n}-{k}' for k in ('input','shadow','mask','output')}
        if not required <= set(case['buffers']): raise ValueError('Missing Sharpen native byte buffers')
        for label,entry in case['buffers'].items():
            raw = files[entry['file']]
            c = case['components'] if label in ('source','source2') else channels
            length = case['width']*case['height']*c
            if label.endswith('mask'):
                length = case['width']*case['height'] if label.startswith('merge') else case['image_width']*case['image_height']
            if len(raw)!=length or len(raw)!=entry['bytes'] or sha(raw)!=entry['sha256']:
                raise ValueError('Changed Sharpen native raster '+entry['file'])
    shapes = json.loads(files['writer-shapes.json'])
    if len(shapes)!=writers: raise ValueError('Wrong old-writer XCF count')
    for shape in shapes:
        if (shape['payload_size']!=68 or shape['procedure_bytes_hex']!=b'plug-in-sharpen\0'.hex() or
            [(r['tag'],r['declared_size']) for r in shape['records']] != [(2,4),(1,0),(10,0),(2,4),(0,0)]):
            raise ValueError('Unexpected genuine old-writer Sharpen shape')
        raw = files[shape['file']][shape['payload_offset']:shape['payload_offset']+shape['payload_size']]
        if sha(raw)!=shape['capsule_sha256']: raise ValueError('Changed genuine old-writer capsule')
    # The inherited startup diagnostics remain visible in each original report.
    # They are not suppressed or represented as a warning-free runtime.
    return report, cases


def read_archive(path, prefix, archive_hash, manifest_hash, archive_limit,
                 byte_limit, member_limit, file_limit, dictionary_bytes):
    path=Path(path)
    if path.is_symlink(): raise ValueError('Symlink evidence archive')
    raw=path.read_bytes(); manifest_raw=path.with_suffix('.manifest.json').read_bytes()
    if len(raw)>archive_limit or sha(raw)!=archive_hash or sha(manifest_raw)!=manifest_hash:
        raise ValueError('Changed or unbounded Sharpen archive/manifest')
    manifest=json.loads(manifest_raw)
    if (manifest['schema_version']!=1 or manifest['compression']!='xz-lzma2' or
        manifest['archive_sha256']!=archive_hash or manifest['dictionary_bytes']!=dictionary_bytes or
        manifest['member_count']!=len(manifest['members']) or manifest['member_count']>member_limit or
        manifest['total_bytes']>byte_limit): raise ValueError('Invalid Sharpen archive manifest')
    files={}
    with tarfile.open(path,'r:xz') as archive:
        for member in archive:
            name=PurePosixPath(member.name)
            if (not member.isfile() or len(name.parts)!=2 or name.parts[0]!=prefix or
                not safe_name(name.name) or name.name in files or name.name not in manifest['members'] or
                member.size>file_limit): raise ValueError('Unsafe Sharpen archive member')
            data=archive.extractfile(member).read(member.size+1); entry=manifest['members'][name.name]
            if len(data)!=member.size or len(data)!=entry['size'] or sha(data)!=entry['sha256']:
                raise ValueError('Changed Sharpen archive member')
            files[name.name]=data
    if set(files)!=set(manifest['members']) or sum(map(len,files.values()))!=manifest['total_bytes']:
        raise ValueError('Incomplete Sharpen corpus')
    return files


def verify(archive=DEFAULT, extract=None):
    base=read_archive(archive,'legacy-sharpen-context',BASE_ARCHIVE_SHA256,BASE_MANIFEST_SHA256,
                      1024*1024,4*1024*1024,2200,1024*1024,8*1024*1024)
    base_report,base_cases=check_capture(base,BASE_REPORT_SHA256,186,72,20)
    large=read_archive(LARGE,'legacy-sharpen-large',LARGE_ARCHIVE_SHA256,LARGE_MANIFEST_SHA256,
                      1024*1024,100*1024*1024,100,5*1024*1024,32*1024*1024)
    large_report,large_cases=check_capture(large,LARGE_REPORT_SHA256,2,2,0)
    if large_report['source_sha256']!=base_report['source_sha256']:
        raise ValueError('Sharpen corpora disagree about pinned transformation sources')
    if {c['id'] for c in base_cases} & {c['id'] for c in large_cases}: raise ValueError('Duplicate combined capture ID')
    if extract is not None:
        target=Path(extract)
        if target.is_symlink() or (target.exists() and any(target.iterdir())):
            raise ValueError('Sharpen materialization target must be absent or empty')
        target.mkdir(parents=True,exist_ok=True)
        for name,raw in base.items(): (target/name).write_bytes(raw)
        evidence=target/'large-evidence'; evidence.mkdir()
        for name,raw in large.items():
            (evidence/name).write_bytes(raw)
            if name.endswith('.raw') and name.startswith('live-'): (target/name).write_bytes(raw)
        # Consumer-only derivative; neither original sealed corpus is modified.
        (target/'captures-base.tsv').write_bytes(base['captures.tsv'])
        (target/'captures.tsv').write_bytes(base['captures.tsv']+large['captures.tsv'])
    return dict(source_commit=COMMIT,case_count=188,pdb_cases=114,live_cases=74,
                old_writer_files=20,large_live_cases=2,base_report_sha256=BASE_REPORT_SHA256,
                large_archive_sha256=LARGE_ARCHIVE_SHA256)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,default=DEFAULT)
    parser.add_argument('--extract',type=Path)
    args=parser.parse_args()
    print(json.dumps(verify(args.archive,args.extract),sort_keys=True))

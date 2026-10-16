#!/usr/bin/env python3
"""Validate and deterministically seal completed genuine Max RGB captures.

This tool never runs a filter or invents observations. It accepts only a full
successful capture with its independently validated source/executable seals.
Printed hashes are review inputs, not automatic activation of archive pins.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import lzma
from pathlib import Path
import tarfile

import check_max_rgb_evidence as base
import check_max_rgb_projection_evidence as projection

KINDS={
    'base':('legacy-max-rgb-context',8*1024*1024,40*1024*1024,5000),
    'large':('legacy-max-rgb-large',32*1024*1024,120*1024*1024,100),
    'projection':('legacy-max-rgb-projection',8*1024*1024,8*1024*1024,1000),
}


def sha(raw): return hashlib.sha256(raw).hexdigest()


def inspect_directory(source,kind):
    source=Path(source)
    if source.is_symlink() or not source.is_dir(): raise ValueError('Expected genuine capture directory')
    files={}
    for path in sorted(source.iterdir()):
        if path.is_symlink() or not path.is_file() or not base.safe_name(path.name):
            raise ValueError('Capture must contain only independent regular files: '+str(path))
        files[path.name]=path.read_bytes()
    if 'capture-report.json' not in files:raise ValueError('Capture has no completed report')
    digest=sha(files['capture-report.json'])
    if kind=='projection': projection.check_capture(files,digest)
    else:base.check_capture(files,digest,kind=='large')
    _,_,byte_limit,member_limit=KINDS[kind]
    if len(files)>member_limit or sum(map(len,files.values()))>byte_limit:
        raise ValueError('Capture exceeds declared finite archive bounds')
    if any(len(raw)>8*1024*1024 for raw in files.values()):
        raise ValueError('Capture has an oversized individual artifact')
    return files,digest


def archive_bytes(files,prefix,dictionary):
    """Canonical ordering, permissions, timestamps and explicit LZMA2 dictionary."""
    raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as tar:
        for name,data in sorted(files.items()):
            member=tarfile.TarInfo(prefix+'/'+name)
            member.size=len(data);member.mode=0o644;member.uid=member.gid=0;member.mtime=0
            member.uname=member.gname=''
            tar.addfile(member,io.BytesIO(data))
    return lzma.compress(raw.getvalue(),format=lzma.FORMAT_XZ,check=lzma.CHECK_CRC64,
                         filters=[dict(id=lzma.FILTER_LZMA2,dict_size=dictionary,preset=6)])


def seal(source,kind,archive):
    archive=Path(archive);manifest_path=archive.with_suffix('.manifest.json')
    if archive.exists() or archive.is_symlink() or manifest_path.exists() or manifest_path.is_symlink():
        raise ValueError('Refusing to replace an existing archive/manifest')
    files,report_hash=inspect_directory(source,kind)
    prefix,dictionary,byte_limit,member_limit=KINDS[kind]
    raw=archive_bytes(files,prefix,dictionary)
    if len(raw)>8*1024*1024:raise ValueError('Compressed archive exceeds verifier bound')
    manifest=dict(schema_version=1,compression='xz-lzma2',archive=archive.name,
                  archive_sha256=sha(raw),dictionary_bytes=dictionary,
                  member_count=len(files),total_bytes=sum(map(len,files.values())),
                  members={name:dict(size=len(data),sha256=sha(data)) for name,data in sorted(files.items())})
    manifest_raw=(json.dumps(manifest,indent=2)+'\n').encode()
    archive.parent.mkdir(parents=True,exist_ok=True)
    with archive.open('xb') as stream:stream.write(raw)
    with manifest_path.open('xb') as stream:stream.write(manifest_raw)
    reread=base.read_archive(archive,prefix,sha(raw),sha(manifest_raw),byte_limit,member_limit,dictionary)
    if reread!=files:raise ValueError('Sealed archive did not round-trip exact captured bytes')
    return dict(kind=kind,archive=str(archive),archive_sha256=sha(raw),
                manifest_sha256=sha(manifest_raw),report_sha256=report_hash,
                member_count=len(files),total_bytes=manifest['total_bytes'],
                dictionary_bytes=dictionary,compressed_bytes=len(raw),
                policy='No pins updated; inspect actual capture and apply these seals explicitly.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind',choices=KINDS,required=True)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--archive',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(seal(args.source,args.kind,args.archive),indent=2))


if __name__=='__main__':main()

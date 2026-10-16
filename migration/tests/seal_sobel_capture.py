#!/usr/bin/env python3
"""Seal an already completed genuine Sobel capture without changing its files.

This does not build, run, or manufacture observations. The complete capture
must pass its separate checker, including native source/output, exact old merge,
writer, phase, copy-history and diagnostic checks. It never updates acceptance
pins: inspect the printed hashes and artifacts independently before pinning.
"""
from pathlib import Path
import argparse
import io
import json
import lzma
import os
import sys
import tarfile
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import check_sobel_evidence as base
import check_sobel_projection_evidence as projection


def seal(directory,archive,kind):
    directory=Path(directory);archive=Path(archive)
    manifest_path=archive.with_suffix('.manifest.json')
    if directory.is_symlink() or not directory.is_dir():raise ValueError('Capture must be a real directory')
    if archive.exists() or archive.is_symlink() or manifest_path.exists() or manifest_path.is_symlink():
        raise ValueError('Refusing to replace any archive or manifest')
    files={}
    for path in directory.iterdir():
        if path.is_symlink() or not path.is_file() or not base.safe_name(path.name):raise ValueError('Unsafe capture artifact')
        if path.stat().st_size>8*1024*1024:raise ValueError('Capture artifact exceeds bound')
        files[path.name]=path.read_bytes()
    limits={'base':(60*1024*1024,8000,8*1024*1024,'legacy-sobel-context'),
            'large':(120*1024*1024,100,32*1024*1024,'legacy-sobel-large'),
            'projection':(8*1024*1024,1400,8*1024*1024,'legacy-sobel-projection')}
    byte_limit,member_limit,dictionary,prefix=limits[kind]
    total=sum(map(len,files.values()))
    if total>byte_limit or len(files)>member_limit:raise ValueError('Capture exceeds corpus resource bounds')
    report_hash=base.sha(files['capture-report.json'])
    if kind=='projection': report,cases,proof=projection.check_capture(files,report_hash)
    else: report,cases=base.check_capture(files,report_hash,kind=='large');proof={}
    archive.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sobel-seal-',dir=archive.parent) as tmp:
        staged=Path(tmp)/archive.name
        with lzma.LZMAFile(staged,'wb',format=lzma.FORMAT_XZ,
            filters=[{'id':lzma.FILTER_LZMA2,'dict_size':dictionary}]) as compressed:
            with tarfile.open(fileobj=compressed,mode='w|',format=tarfile.GNU_FORMAT) as tar:
                for name,raw in sorted(files.items()):
                    member=tarfile.TarInfo(prefix+'/'+name)
                    member.size=len(raw);member.mtime=0;member.uid=member.gid=0
                    member.uname=member.gname='';member.mode=0o644
                    tar.addfile(member,io.BytesIO(raw))
        archive_hash=base.sha(staged.read_bytes())
        manifest=dict(schema_version=1,compression='xz-lzma2',dictionary_bytes=dictionary,
            archive_sha256=archive_hash,member_count=len(files),total_bytes=total,
            members={name:dict(size=len(raw),sha256=base.sha(raw)) for name,raw in sorted(files.items())})
        staged_manifest=staged.with_suffix('.manifest.json')
        staged_manifest.write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
        manifest_hash=base.sha(staged_manifest.read_bytes())
        restored=base.read_archive(staged,prefix,archive_hash,manifest_hash,byte_limit,member_limit,dictionary)
        if restored!=files:raise ValueError('Sealed archive did not reproduce every original byte')
        # Exclusive creation avoids silently replacing a concurrently published
        # artifact. A partial failure leaves evidence visible, never retries it.
        with archive.open('xb') as stream:stream.write(staged.read_bytes())
        with manifest_path.open('xb') as stream:stream.write(staged_manifest.read_bytes())
    return dict(kind=kind,case_count=len(cases),archive=str(archive),archive_sha256=archive_hash,
                manifest=str(manifest_path),manifest_sha256=manifest_hash,report_sha256=report_hash,
                member_count=len(files),total_bytes=total,proof=proof,
                acceptance_pins_changed=False)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture-directory',type=Path,required=True)
    p.add_argument('--archive',type=Path,required=True)
    p.add_argument('--kind',choices=('base','large','projection'),required=True)
    a=p.parse_args()
    print(json.dumps(seal(a.capture_directory,a.archive,a.kind),indent=2,sort_keys=True))

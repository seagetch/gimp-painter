#!/usr/bin/env python3
"""Split or reassemble a local artifact with per-part and whole-file SHA-256 seals."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''):
            h.update(data)
    return h.hexdigest()


def join(directory, destination):
    manifest=json.loads((directory/'parts.json').read_text())
    if destination.exists():
        raise ValueError('Destination exists; refusing to overwrite it')
    original_name=manifest['original_name']
    if not original_name or Path(original_name).name!=original_name or original_name in {'.','..'}:
        raise ValueError('Unsafe original artifact name')
    prefix=original_name+'.part-'
    expected_names=[f'{prefix}{i:03d}' for i in range(1,len(manifest['parts'])+1)]
    if [p['name'] for p in manifest['parts']] != expected_names:
        raise ValueError('Part names/order are not canonical')
    for part in manifest['parts']:
        source=directory/part['name']
        if source.is_symlink() or source.stat().st_size!=part['bytes'] or digest(source)!=part['sha256']:
            raise ValueError('Missing, changed or unsafe part: '+part['name'])
    whole=hashlib.sha256()
    with destination.open('xb') as output:
        for part in manifest['parts']:
            with (directory/part['name']).open('rb') as source:
                for block in iter(lambda:source.read(1024*1024),b''):
                    whole.update(block)
                    output.write(block)
    if destination.stat().st_size!=manifest['bytes'] or whole.hexdigest()!=manifest['sha256']:
        raise ValueError('Reassembled file hash/size mismatch; do not use output')
    return {'path':str(destination),'bytes':destination.stat().st_size,'sha256':whole.hexdigest()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    subs=parser.add_subparsers(dest='action',required=True)
    split=subs.add_parser('split')
    split.add_argument('source',type=Path)
    split.add_argument('--output',type=Path,required=True)
    split.add_argument('--part-bytes',type=int,default=19_000_000)
    assemble=subs.add_parser('join')
    assemble.add_argument('directory',type=Path)
    assemble.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.action=='join':
        print(json.dumps(join(args.directory,args.output)))
        return
    if not 0<args.part_bytes<20_000_000:
        parser.error('Every part must be smaller than20MB')
    args.output.mkdir(parents=True,exist_ok=False)
    manifest={'format':1,'original_name':args.source.name,'bytes':args.source.stat().st_size,
              'sha256':digest(args.source),'parts':[]}
    with args.source.open('rb') as source:
        index=1
        while block:=source.read(args.part_bytes):
            name=f'{args.source.name}.part-{index:03d}'
            path=args.output/name
            path.write_bytes(block)
            manifest['parts'].append({'name':name,'bytes':len(block),'sha256':digest(path)})
            index+=1
    (args.output/'parts.json').write_text(json.dumps(manifest,indent=2)+'\n')
    shutil.copy2(Path(__file__),args.output/'reassemble.py')
    (args.output/'README.txt').write_text(
        'Keep every part together with parts.json and reassemble.py.\n'
        'These parts are local review artifacts; package/source completeness and licensing\n'
        'remain governed by the original package manifest.\n\n'
        'From this directory, verify and reassemble with Python3:\n'
        f'  python3 reassemble.py join . --output "{args.source.name}"\n\n'
        'The command refuses to overwrite existing files, checks each part, then verifies\n'
        'the complete SHA-256 and byte length before reporting success.\n')
    # Verify concatenated bytes without creating an extra copy of the archive.
    assembled=hashlib.sha256()
    for part in manifest['parts']:
        with (args.output/part['name']).open('rb') as stream:
            for block in iter(lambda:stream.read(1024*1024),b''):
                assembled.update(block)
    if assembled.hexdigest()!=manifest['sha256']:
        raise ValueError('Split/reassembly verification failed')
    print(json.dumps({'parts':len(manifest['parts']),'bytes':manifest['bytes'],
                      'sha256':manifest['sha256'],'reassembly_verified':True}))


if __name__=='__main__':
    main()

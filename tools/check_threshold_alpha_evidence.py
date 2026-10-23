#!/usr/bin/env python3
"""Validate and materialize sealed genuine Threshold Alpha evidence.

Capture validation is deliberately separate from current-port parity. Only
synthetic old-runtime observations enter these archives. No unchecked corpus
can activate the native old-XCF tests. Pins stay empty until a real capture has
passed and its independently inspected artifacts have been sealed.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import tarfile

ROOT = Path(__file__).resolve().parents[1]
COMMIT = 'afa43fae3e920210146abed514f136fd49f671b5'
DEFAULT = ROOT/'migration/fixtures/legacy-threshold-alpha-context.tar.xz'
LARGE = ROOT/'migration/fixtures/legacy-threshold-alpha-large.tar.xz'
# Populated from separately completed and sealed genuine captures only.
BASE_ARCHIVE_SHA256 = '2900b8082f0175123c49f02e940a23a32cbd7c4d7e803e88a4e1e06b6b64c0bf'
BASE_MANIFEST_SHA256 = '0e49a3ef3cfbc2003b7b8a47dd0b1af1a086673b76a72afd2b8fcaae7ab93961'
BASE_REPORT_SHA256 = '10ff7c852a47af1f91121911be300a4f653f279ba7729db476f9b3213da3be12'
LARGE_ARCHIVE_SHA256 = '218e75c0bb6f333afc49e4ab26a5a6bda614770a67a675e46ff7395b955c2e73'
LARGE_MANIFEST_SHA256 = '3095578ca402ade08e220e5ea0056062069c6f933b2369162fc4c5ebc6d55876'
LARGE_REPORT_SHA256 = 'c7733f0e09813c2d809f9f6e083e15dafdcf34ac2bdf63f1719de181f4c98ff2'
COLUMNS = ('id live source_channels output_channels width height image_width image_height '
           'offset_x offset_y threshold start_selection final_selection start_components '
           'final_components start_lock final_lock rerun writer starts ends merges status mutated '
           'phase idle_selection idle_components idle_lock idle_starts idle_ends').split()
LITERAL = b'plug-in-threshold-alpha\0'
WRITER_FIELDS = [(2,4),(1,0),(10,0),(2,4),(0,0)]


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def safe_name(name):
    return bool(name) and name == Path(name).name and name not in ('.', '..') and '\\' not in name


def table(files, name, columns):
    rows = [line.split('\t') for line in files[name].decode().splitlines()]
    if any(len(row) != columns for row in rows): raise ValueError('Invalid '+name+' row shape')
    return rows


def inspect_writer(files, case):
    """Decode the actual writer file independently of the capture script."""
    name = case['id']+'.xcf'; data = files[name]
    needle = struct.pack('>I',len(LITERAL))+LITERAL
    at = data.find(needle)
    if at < 8 or data.find(needle,at+1) != -1: raise ValueError('Nonunique actual writer procedure')
    tag,size = struct.unpack_from('>II',data,at-8)
    if (tag,size) != (32,76): raise ValueError('Unexpected actual Threshold Alpha capsule size')
    raw=data[at:at+size]
    if len(raw)!=76: raise ValueError('Truncated actual writer capsule')
    pos=len(needle); records=[]
    for expected in WRITER_FIELDS:
        tag,length=struct.unpack_from('>II',raw,pos); pos+=8
        if (tag,length)!=expected: raise ValueError('Unexpected actual Threshold Alpha argument framing')
        payload=raw[pos:pos+length]; pos+=length
        records.append(dict(tag=tag,declared_size=length,payload_hex=payload.hex()))
    if pos!=len(raw): raise ValueError('Actual writer tail')
    if struct.unpack_from('>i',raw,64)[0]!=case['threshold']:
        raise ValueError('Actual writer changed signed threshold')
    if files.get(case['id']+'.definition.raw')!=raw:
        raise ValueError('Retained writer capsule is not the actual XCF bytes')
    return dict(file=name,property_offset=at-8,payload_offset=at,payload_size=size,
                procedure_bytes_hex=LITERAL.hex(),records=records,capsule_sha256=sha(raw))


def generated_source(case, variant):
    """Independently replay only the synthetic input generator, not a filter."""
    width=case['width']; height=case['height']; channels=case['components']
    alpha=(0,1,126,127,128,254,255,63)
    result=bytearray(width*height*channels)
    for y in range(height):
        for x in range(width):
            for c in range(channels):
                if c==channels-1 and channels%2==0:
                    value=0 if case['zero_alpha'] and variant==0 else alpha[(x+3*y+variant*3)%8]
                else: value=(x*(37+c*17)+y*(61+c*23)+variant*97+c*51+11)%256
                result[(y*width+x)*channels+c]=value
    return bytes(result)


def generated_mask(case, kind):
    width=case['image_width']; height=case['image_height']; ox=case['offset_x']; oy=case['offset_y']
    result=bytearray(width*height)
    for y in range(height):
        for x in range(width):
            value=0
            if kind==1 and width//4<=x<3*width//4 and height//4<=y<3*height//4: value=255
            if kind==2 and 0<x<width-1 and 0<y<height-1: value=(x*17+y*29)%256
            if kind==3 and x==width-1 and y==height-1: value=255
            if kind==4 and 0<x<width-1 and 0<y<height-1: value=255
            if kind==5 and ox+16<=x<ox+112 and oy+16<=y<oy+112: value=255
            if kind==6 and ox+63<=x<ox+65 and oy+63<=y<oy+65: value=255
            if kind==7 and x==ox+1 and y==oy+1: value=255
            result[y*width+x]=value
    return bytes(result)


def observations(files, tests):
    """Check execution events, byte provenance, raw shadow and stable colors."""
    rows=table(files,'captures.tsv',len(COLUMNS))
    observed={row[0]:dict(zip(COLUMNS,[row[0]]+list(map(int,row[1:])))) for row in rows}
    if len(observed)!=len(rows) or set(observed)!={c['id'] for c in tests}:
        raise ValueError('Duplicate/missing actual Threshold Alpha cases')
    event_rows=table(files,'events.tsv',6)
    merge_rows=table(files,'merges.tsv',7)
    merge_context={r[0]:tuple(map(int,r[1:])) for r in merge_rows}
    if len(merge_context)!=len(merge_rows): raise ValueError('Repeated merge context row')
    roi_rows=table(files,'rois.tsv',7)
    rois={(r[0],int(r[1])):tuple(map(int,r[2:])) for r in roi_rows}
    if len(rois)!=len(roi_rows): raise ValueError('Repeated captured run ROI')
    records=[]; shapes=[]
    for test in tests:
        ident=test['id']; row=observed[ident]
        c=(2 if test['components']<=2 else 4) if test['live'] else test['components']
        rerun=bool(test['rerun']); runs=1+rerun if test['live'] else 0
        rejection=not test['live'] and c%2==1
        merges=0 if rejection or test['selection']==3 else 1+rerun
        status=-1 if test['live'] else 0 if rejection else 3
        if (row['starts'],row['ends'],row['merges'],row['status'])!=(runs,runs,merges,status):
            raise ValueError('Wrong actual run/merge/rejection counts '+ident)
        if row['output_channels']!=c or not -2147483648<=test['threshold']<=2147483647:
            raise ValueError('Invalid captured layout or signed threshold '+ident)
        for observed_key,case_key in (('width','width'),('height','height'),('threshold','threshold'),
            ('source_channels','components'),('phase','phase'),('rerun','rerun'),
            ('start_selection','selection'),('final_selection','final_selection'),
            ('start_components','active_components'),('final_components','final_components'),
            ('start_lock','lock_alpha'),('final_lock','final_lock'),
            ('idle_selection','idle_selection'),('idle_components','idle_components'),('idle_lock','idle_lock')):
            if row[observed_key]!=test[case_key]: raise ValueError('Capture row/plan disagreement '+ident)
        idle=any(test[k]>=0 for k in ('idle_selection','idle_components','idle_lock'))
        if (row['idle_starts'],row['idle_ends'])!=((1,1) if idle else (-1,-1)):
            raise ValueError('Unsolicited idle context run '+ident)
        events=[r for r in event_rows if r[0]==ident]
        mutations=[(i,r) for i,r in enumerate(events) if r[1]=='progress-mutate']
        if len(mutations)!=bool(test['phase']) or row['mutated']!=bool(test['phase']):
            raise ValueError('Missing or repeated actual mutation hook '+ident)
        if test['phase']:
            index,event=mutations[0]; p=float(event[4])
            if (test['phase']==1 and p!=1) or (test['phase']==2 and not 0<p<1):
                raise ValueError('Wrong actual progress phase '+ident)
            if int(event[2])!=1 or int(event[3])!=0 or int(event[5])!=0 or any(
                r[1] in ('merge-before','merge-after','end') for r in events[:index]):
                raise ValueError('Mutation did not precede merge/end '+ident)
            if test['selection'] in (5,6) and p!=0.25:
                raise ValueError('Tile-crossing first observed progress was not .25 '+ident)
        for n in range(1,runs+1):
            if sum(r[1]=='start' and int(r[2])==n for r in events)!=1 or sum(
                r[1]=='end' and int(r[3])==n for r in events)!=1:
                raise ValueError('Missing exact start/end observation '+ident)
        if sum(r[1]=='merge-before' for r in events)!=merges or sum(
            r[1]=='merge-after' for r in events)!=merges:
            raise ValueError('Missing exact merge observations '+ident)
        required={'source','input','output','start-mask','final-mask'}
        if test['live']: required|={'start1','start1-mask'}
        if rerun: required|={'source2','output2','start2','start2-mask'}
        if idle: required|={'idle-output','idle-mask'}
        for n in range(1,merges+1): required|={f'merge{n}-{k}' for k in ('input','shadow','mask','output')}
        buffers={}
        for label in sorted(required):
            filename=ident+'-'+label+'.raw'; raw=files[filename]
            channels=test['components'] if label in ('source','source2') else c
            length=test['width']*test['height']*channels
            if label.endswith('mask'):
                length=(test['width']*test['height'] if label.startswith('merge') else
                        test['image_width']*test['image_height'])
            if len(raw)!=length: raise ValueError('Wrong raster extent '+filename)
            buffers[label]=dict(file=filename,bytes=length,sha256=sha(raw))
        def pixels(label): return files[buffers[label]['file']]
        if pixels('source')!=generated_source(test,0):
            raise ValueError('Source raster is not the deterministic synthetic generator '+ident)
        if rerun and pixels('source2')!=generated_source(test,int(test['rerun']==1)):
            raise ValueError('Second source raster is not the declared synthetic generator '+ident)
        final_kind=test['final_selection'] if test['final_selection']>=0 else test['selection']
        idle_kind=test['idle_selection'] if test['idle_selection']>=0 else final_kind
        mask_kinds={'start-mask':test['selection'],'final-mask':final_kind}
        if test['live']: mask_kinds['start1-mask']=test['selection']
        if rerun: mask_kinds['start2-mask']=idle_kind
        if idle: mask_kinds['idle-mask']=idle_kind
        for label,kind in mask_kinds.items():
            if pixels(label)!=generated_mask(test,kind):
                raise ValueError('Mask is not the deterministic synthetic generator '+ident+' '+label)
        if (rejection or test['selection']==3) and pixels('input')!=pixels('output'):
            raise ValueError('Rejected or no-merge invocation changed drawable '+ident)
        if test['rerun']==1 and pixels('source')==pixels('source2'):
            raise ValueError('Changed-source control did not change bytes '+ident)
        if test['rerun']==2 and pixels('source')!=pixels('source2'):
            raise ValueError('Unchanged-source notification altered pixels '+ident)
        if idle and pixels('idle-output')!=pixels('output'):
            raise ValueError('Idle context edit altered settled cache '+ident)
        for n in range(1,(1+rerun)+1):
            selected,x1,y1,x2,y2=rois[ident,n]
            if n==1 and test['selection']==5 and (x1,y1,x2,y2)!=(16,16,112,112):
                raise ValueError('Wrong actual 96x96 crossing ROI '+ident)
            if n==1 and test['selection']==6 and (x1,y1,x2,y2)!=(63,63,65,65):
                raise ValueError('Wrong actual 2x2 crossing ROI '+ident)
            if (x2-x1)*(y2-y1)>2147483647: raise ValueError('Undefined signed iterator area '+ident)
            if n>merges: continue
            inp=pixels(f'merge{n}-input'); shadow=pixels(f'merge{n}-shadow'); output=pixels(f'merge{n}-output')
            bits=test['final_components'] if test['final_components']>=0 else test['active_components']
            lock=test['final_lock'] if test['final_lock']>=0 else test['lock_alpha']
            final_selection=test['final_selection'] if test['final_selection']>=0 else test['selection']
            if n==2:
                if test['idle_components']>=0: bits=test['idle_components']
                if test['idle_lock']>=0: lock=test['idle_lock']
                if test['idle_selection']>=0: final_selection=test['idle_selection']
            effective_bits=bits & ~(1 << (c-1)) if lock else bits
            if merge_context[ident+f'-merge{n}']!=(test['width'],test['height'],c,effective_bits,int(final_selection==0),lock):
                raise ValueError('Actual final merge components/lock/selection differ '+ident)
            if inp!=pixels('input' if n==1 else 'start2') or output!=pixels('output' if n==1 else 'output2'):
                raise ValueError('Merge and observed drawable buffers disagree '+ident)
            global_mask=pixels('idle-mask' if n==2 and idle else 'final-mask')
            coverage=pixels(f'merge{n}-mask')
            for y in range(test['height']):
                for x in range(test['width']):
                    ix=x+test['offset_x']; iy=y+test['offset_y']
                    expected_coverage=255 if final_selection==0 else (global_mask[iy*test['image_width']+ix]
                        if 0<=ix<test['image_width'] and 0<=iy<test['image_height'] else 0)
                    if coverage[y*test['width']+x]!=expected_coverage:
                        raise ValueError('Actual local final mask disagrees with image mask/offset '+ident)

            width=test['width']
            for offset in range(0,len(inp),c):
                index=offset//c; x=index%width; y=index//width
                expected=(inp[offset:offset+c-1]+bytes([255 if test['threshold']<inp[offset+c-1] else 0])
                          if x1<=x<x2 and y1<=y<y2 else bytes(c))
                if shadow[offset:offset+c]!=expected:
                    raise ValueError('Actual shadow differs from start ROI/point equation '+ident+' '+str(n)+' '+str(index))
                if output[offset:offset+c-1]!=inp[offset:offset+c-1]:
                    raise ValueError('Actual merged color invariant failed '+ident+' '+str(n)+' '+str(index))
        record=dict(test,observed=row,buffers=buffers)
        if test['writer']:
            shape=inspect_writer(files,test); record['writer']=shape; shapes.append(shape)
        records.append(record)
    return records,shapes


def validate_directory(out, tests, code):
    out=Path(out); errors=[]; records=[]; shapes=[]
    files={p.name:p.read_bytes() for p in out.iterdir() if p.is_file()}
    if code: errors.append('Harness exit '+str(code))
    if b'LEGACY_THRESHOLD_ALPHA_CAPTURE_COMPLETE' not in files.get('capture.log',b''):
        errors.append('Missing successful completion marker')
    # Extract the actual original capsules first, then independently decode all
    # framing, tags, lengths, signed payloads and terminal/tail boundaries.
    from capture_legacy_threshold_alpha import writer_shape
    for case in tests:
        if case['writer']:
            try: writer_shape(out/(case['id']+'.xcf'))
            except (OSError,ValueError,struct.error) as exc: errors.append(str(exc))
    files={p.name:p.read_bytes() for p in out.iterdir() if p.is_file()}
    try: records,shapes=observations(files,tests)
    except (KeyError,ValueError,struct.error) as exc: errors.append(str(exc))
    fixture=dict(schema_version=1,capture_columns=COLUMNS,cases=records,
        selection_kinds={'0':'empty/no selection','1':'center hard','2':'soft','3':'outside drawable',
                         '4':'expanded hard','5':'local [16,112)^2','6':'local [63,65)^2','7':'local 1x1 at (1,1)'},
        phase_kinds={'0':'none','1':'pre_merge: actual progress=1 before merge','2':'mid_work: actual 0<progress<1 before merge'},
        rerun_kinds={'0':'none','1':'changed lower-source bytes','2':'unchanged-byte lower-source notification'},
        roi_columns='id run selected x1 y1 x2 y2'.split(),
        event_columns='id event starts ends progress merge_count'.split(),
        layout_policy='PDB Y/RGB rejects without mutation; genuine live FilterLayer promotes Y/RGB input to YA/RGBA.',
        status_policy='PDB status is observed; async live -1 is unavailable, with exact start/end/merge evidence.',
        mask_policy='start/final/idle masks image-sized; merge masks drawable-local effective coverage.',
        diagnostics_policy='Inherited legacy diagnostics retained in capture report; no warning-free runtime claim.')
    (out/'fixtures.json').write_text(json.dumps(fixture,indent=2)+'\n')
    (out/'writer-shapes.json').write_text(json.dumps(shapes,indent=2)+'\n')
    return errors,len(records)


def read_archive(path,prefix,archive_hash,manifest_hash,byte_limit,member_limit,dictionary_bytes):
    path=Path(path)
    if not archive_hash or not manifest_hash: raise ValueError('Threshold Alpha capture has not yet been sealed')
    if path.is_symlink(): raise ValueError('Symlink evidence archive')
    raw=path.read_bytes(); manifest_raw=path.with_suffix('.manifest.json').read_bytes()
    if len(raw)>8*1024*1024 or sha(raw)!=archive_hash or sha(manifest_raw)!=manifest_hash:
        raise ValueError('Changed or unbounded Threshold Alpha archive/manifest')
    manifest=json.loads(manifest_raw)
    if (manifest['schema_version']!=1 or manifest['compression']!='xz-lzma2' or
        manifest['archive_sha256']!=archive_hash or manifest['dictionary_bytes']!=dictionary_bytes or
        manifest['member_count']!=len(manifest['members']) or manifest['member_count']>member_limit or
        manifest['total_bytes']>byte_limit): raise ValueError('Invalid Threshold Alpha archive manifest')
    files={}; total=0
    with tarfile.open(path,'r:xz') as archive:
        for member in archive:
            name=PurePosixPath(member.name)
            if (not member.isfile() or len(name.parts)!=2 or name.parts[0]!=prefix or
                not safe_name(name.name) or name.name in files or name.name not in manifest['members'] or
                member.size>8*1024*1024 or total+member.size>byte_limit):
                raise ValueError('Unsafe Threshold Alpha archive member')
            data=archive.extractfile(member).read(member.size+1); entry=manifest['members'][name.name]
            if len(data)!=member.size or len(data)!=entry['size'] or sha(data)!=entry['sha256']:
                raise ValueError('Changed Threshold Alpha archive member')
            files[name.name]=data; total+=len(data)
    if set(files)!=set(manifest['members']) or total!=manifest['total_bytes']:
        raise ValueError('Incomplete Threshold Alpha corpus')
    return files


def check_capture(files,report_hash,large):
    if not report_hash or sha(files['capture-report.json'])!=report_hash:
        raise ValueError('Changed sealed Threshold Alpha report')
    report=json.loads(files['capture-report.json'])
    if (report['status']!='passed' or report['errors'] or report['exit_code']!=0 or
        report['source_commit']!=COMMIT or report['large_cases']!=large or
        report['source_sha256']!=report['post_capture_source_sha256'] or
        report['executable_sha256']!=report['post_capture_executable_sha256']):
        raise ValueError('Invalid Threshold Alpha capture provenance')
    if set(files)!=set(report['files_sha256'])|{'capture-report.json'}:
        raise ValueError('Unrecorded or missing Threshold Alpha artifact')
    for name,digest in report['files_sha256'].items():
        if not safe_name(name) or sha(files[name])!=digest: raise ValueError('Changed Threshold Alpha artifact '+name)
    for name,key in (('capture-script.py','capture_script_sha256'),('capture-harness.c','harness_sha256'),
                     ('observation-hooks.patch','instrumentation_sha256'),('observation-helper.py','observation_helper_sha256'),
                     ('evidence-validator.py','evidence_validator_sha256')):
        if sha(files[name])!=report[key]: raise ValueError('Changed Threshold Alpha capture source '+name)
    if b'LEGACY_THRESHOLD_ALPHA_CAPTURE_COMPLETE' not in gzip.decompress(files['capture.log.gz']):
        raise ValueError('No successful genuine capture completion')
    tests=json.loads(files['capture-plan.json'])
    expected=(2,2,0) if large else (256,172,28)
    if (len(tests),sum(bool(c['live']) for c in tests),sum(bool(c['writer']) for c in tests))!=expected:
        raise ValueError('Incomplete bounded Threshold Alpha coverage matrix')
    if report['case_count']!=len(tests) or report['expected_cases']!=len(tests):
        raise ValueError('Wrong Threshold Alpha capture counts')
    records,shapes=observations(files,tests)
    fixture=json.loads(files['fixtures.json'])
    if fixture['capture_columns']!=COLUMNS or records!=fixture['cases'] or shapes!=json.loads(files['writer-shapes.json']):
        raise ValueError('Threshold Alpha captured observations and indexes disagree')
    return report,records


def verify(archive=DEFAULT,extract=None):
    base=read_archive(archive,'legacy-threshold-alpha-context',BASE_ARCHIVE_SHA256,BASE_MANIFEST_SHA256,
                      40*1024*1024,5000,8*1024*1024)
    base_report,base_cases=check_capture(base,BASE_REPORT_SHA256,False)
    large=read_archive(LARGE,'legacy-threshold-alpha-large',LARGE_ARCHIVE_SHA256,LARGE_MANIFEST_SHA256,
                       120*1024*1024,100,32*1024*1024)
    large_report,large_cases=check_capture(large,LARGE_REPORT_SHA256,True)
    if base_report['source_sha256']!=large_report['source_sha256']:
        raise ValueError('Threshold Alpha corpora disagree about pinned sources')
    if {c['id'] for c in base_cases}&{c['id'] for c in large_cases}: raise ValueError('Duplicate capture ID')
    if extract is not None:
        target=Path(extract)
        if target.is_symlink() or (target.exists() and any(target.iterdir())):
            raise ValueError('Threshold Alpha target must be absent or empty')
        target.mkdir(parents=True,exist_ok=True)
        for name,raw in base.items(): (target/name).write_bytes(raw)
        evidence=target/'large-evidence'; evidence.mkdir()
        for name,raw in large.items():
            (evidence/name).write_bytes(raw)
            if name.endswith('.raw') and name.startswith('live-'): (target/name).write_bytes(raw)
        (target/'captures-base.tsv').write_bytes(base['captures.tsv'])
        (target/'captures.tsv').write_bytes(base['captures.tsv']+large['captures.tsv'])
        (target/'rois.tsv').write_bytes(base['rois.tsv']+large['rois.tsv'])
    cases=base_cases+large_cases
    return dict(source_commit=COMMIT,case_count=len(cases),pdb_cases=sum(not c['live'] for c in cases),
                live_cases=sum(bool(c['live']) for c in cases),old_writer_files=sum(bool(c['writer']) for c in cases),
                large_live_cases=len(large_cases),base_report_sha256=BASE_REPORT_SHA256,
                large_archive_sha256=LARGE_ARCHIVE_SHA256)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,default=DEFAULT)
    parser.add_argument('--extract',type=Path)
    args=parser.parse_args()
    print(json.dumps(verify(args.archive,args.extract),sort_keys=True))

#!/usr/bin/env python3
"""Validate and materialize sealed genuine Max RGB evidence.

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
DEFAULT = ROOT/'migration/fixtures/legacy-max-rgb-context.tar.xz'
LARGE = ROOT/'migration/fixtures/legacy-max-rgb-large.tar.xz'
# Populated from separately completed and sealed genuine captures only.
BASE_ARCHIVE_SHA256 = '67c571b0353d449bfed89c6f7a0780c8ea176adb0a616bd6f66ae89192ff0b28'
BASE_MANIFEST_SHA256 = '77a7a9d8d45152f1dc56f28cdf26cf4da8ac8bf9045114647a2f24625d69be79'
BASE_REPORT_SHA256 = '2540c736fb01a249bb47145ff1c21c44263165c967f2125521f5b72a3aeac9dd'
LARGE_ARCHIVE_SHA256 = 'ed6abdcfe75be94fc6971fa400f9607386ba047a717f009e6b3a1df34dff0306'
LARGE_MANIFEST_SHA256 = '6fa51b03fa776e25d9b96e22be5e3a89a7a5c76f24fcf8d69f84940cd5fc065c'
LARGE_REPORT_SHA256 = '6cee0c54271c7c8663cc585da7584a2644318f5b51135904e149c7e5c53c4d70'
COLUMNS = ('id live source_channels output_channels width height image_width image_height '
           'offset_x offset_y mode start_selection final_selection start_components '
           'final_components start_lock final_lock rerun writer starts ends merges status mutated '
           'phase idle_selection idle_components idle_lock idle_starts idle_ends').split()
LITERAL = b'plug-in-max-rgb\0'
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
    if (tag,size) != (32,68): raise ValueError('Unexpected actual Max RGB capsule size')
    raw=data[at:at+size]
    if len(raw)!=68: raise ValueError('Truncated actual writer capsule')
    pos=len(needle); records=[]
    for expected in WRITER_FIELDS:
        tag,length=struct.unpack_from('>II',raw,pos); pos+=8
        if (tag,length)!=expected: raise ValueError('Unexpected actual Max RGB argument framing')
        payload=raw[pos:pos+length]; pos+=length
        records.append(dict(tag=tag,declared_size=length,payload_hex=payload.hex()))
    if pos!=len(raw): raise ValueError('Actual writer tail')
    if struct.unpack_from('>i',raw,28)[0] != 1:
        raise ValueError('Actual writer changed noninteractive run mode')
    if struct.unpack_from('>i',raw,56)[0]!=case['mode']:
        raise ValueError('Actual writer changed signed mode')
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
    palette=((0,0,0),(255,255,255),(90,90,90),(200,200,20),
             (20,20,200),(200,20,200),(20,200,20),(0,128,255))
    for y in range(height):
        for x in range(width):
            index=(x+3*y+variant*5)%16
            if index<8: result[(y*width+x)*channels:(y*width+x)*channels+3]=bytes(palette[index])
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


def check_signature(files):
    expected=[['0','run-mode','GimpParamInt32','GimpInt32'],
              ['1','image','GimpParamImageID','GimpImageID'],
              ['2','drawable','GimpParamDrawableID','GimpDrawableID']]
    expected += [[str(i),name,'GimpParamInt32','GimpInt32'] for i,name in
                 enumerate(('max-p',),3)]
    if table(files,'signature.tsv',4)!=expected:
        raise ValueError('Original Max RGB does not expose the expected four arguments with a signed INT32 mode')


def mask_roi(mask,iw,ih,ox,oy,w,h):
    left,top,right,bottom=iw,ih,0,0
    for y in range(ih):
        row=mask[y*iw:(y+1)*iw]
        nonzero=[x for x,v in enumerate(row) if v]
        if nonzero:
            left=min(left,nonzero[0]);right=max(right,nonzero[-1]+1)
            top=min(top,y);bottom=y+1
    if left==iw:return (0,0,0,w,h)
    return (1,max(0,min(w,left-ox)),max(0,min(h,top-oy)),
            max(0,min(w,right-ox)),max(0,min(h,bottom-oy)))


def check_merge(original,shadow,coverage,output,channels,active):
    """Check pinned paint-funcs.c replace_inten_pixels, opacity 255.

    This separately transcribes the observed old integer macros. It never
    substitutes generated bytes for any captured artifact. Python's signed >>
    has the pinned old compiler's arithmetic-shift semantics for INT_MULT.
    """
    def mult(a,b):
        t=a*b+128
        return ((t>>8)+t)>>8
    def divide(a,b):
        return a//b+int(a%b>b//2)
    alpha=channels-(channels%2==0)
    if len(original)!=len(shadow) or len(output)!=len(original) or len(coverage)*channels!=len(output):
        raise ValueError('Invalid captured merge extents')
    hidden=0
    for pixel,mask in enumerate(coverage):
        at=pixel*channels
        old_alpha=original[at+alpha] if alpha<channels else 255
        shadow_alpha=shadow[at+alpha] if alpha<channels else 255
        mixed=old_alpha+mult(shadow_alpha-old_alpha,mult(mask,255))
        ratio=divide(mask*shadow_alpha,mixed) if mixed else 0
        for channel in range(alpha):
            old=original[at+channel];new=shadow[at+channel];expected=old
            if mixed and (active&(1<<channel)):
                amount=divide(abs(new-old)*ratio,255)
                expected+=amount if new>old else -amount
            # Old destination is guchar. Rare low-alpha ratios overshoot 255;
            # preserve the defined unsigned-byte conversion, not a clamp.
            expected &= 255
            if output[at+channel]!=expected:
                raise ValueError('Captured final merge differs from old integer REPLACE at '+str(pixel))
            if not mixed and old!=new: hidden+=1
        if alpha<channels:
            expected=mixed if active&(1<<alpha) else old_alpha
            if output[at+alpha]!=expected:
                raise ValueError('Captured final merge alpha differs from old integer REPLACE at '+str(pixel))
    return hidden


def check_runtime_diagnostics(raw):
    """Retain known old startup diagnostics; reject additional diagnostics."""
    lines=raw.decode(errors='replace').splitlines()
    diagnostics=[line for line in lines if any(word in line.lower() for word in
        ('critical','warning','error','assertion','failed'))]
    known=[];unexpected=[]
    object_warning="g_value_set_object: assertion 'G_VALUE_HOLDS_OBJECT (value)' failed"
    for line in diagnostics:
        if object_warning in line or line=='Failed to parse tag cache: No such file or directory':
            known.append(line)
        else: unexpected.append(line)
    if sum(object_warning in line for line in known)>2 or sum(line.startswith('Failed to parse tag cache:') for line in known)>1:
        unexpected.append('Repeated ordinary startup diagnostics beyond known startup counts')
    if unexpected: raise ValueError('Unexpected legacy runtime diagnostics: '+repr(unexpected))
    return diagnostics


def raw_pixel(pixel, mode):
    """Independent point equation; only RGB/RGBA, signed mode >0 or <=0."""
    if len(pixel) not in (3,4): raise ValueError('Max RGB callback requires RGB/RGBA')
    if type(mode) is not int or not -2147483648<=mode<=2147483647:
        raise ValueError('Mode must be a signed INT32')
    chosen=(max if mode>0 else min)(pixel[:3])
    return bytes(v if v==chosen else 0 for v in pixel[:3])+bytes(pixel[3:])


def check_phase(events, phase, mutated, ident):
    """Require observed hooks; image size or progress=1 is never phase proof."""
    if phase not in (0,1,2): raise ValueError('Unknown requested phase '+ident)
    mutations=[(i,r) for i,r in enumerate(events) if r[1].endswith('-mutate')]
    if len(mutations)!=bool(phase) or mutated!=bool(phase):
        raise ValueError('Missing or repeated actual mutation hook '+ident)
    if not phase: return
    index,event=mutations[0]; p=float(event[4])
    if event[1]!={1:'merge-entry-mutate',2:'progress-mid-mutate'}[phase]:
        raise ValueError('Wrong actual mutation phase '+ident)
    if (phase==1 and p!=0) or (phase==2 and not 0<p<1):
        raise ValueError('Wrong actual progress phase '+ident)
    if int(event[2])!=1 or int(event[3])!=0 or int(event[5])!=0 or any(
        r[1] in ('merge-before','merge-after','end') for r in events[:index]):
        raise ValueError('Mutation did not precede merge/end '+ident)
    if phase==1 and (index+1>=len(events) or events[index+1][1]!='merge-before'):
        raise ValueError('Mutation did not occur at actual core merge entry '+ident)
    if phase==2 and (index==0 or events[index-1][1]!='progress' or float(events[index-1][4])!=p):
        raise ValueError('Mutation lacks its actual PDB progress request '+ident)


def observations(files, tests):
    """Check execution events, byte provenance, raw shadow and stable colors."""
    check_signature(files)
    rows=table(files,'captures.tsv',len(COLUMNS))
    observed={row[0]:dict(zip(COLUMNS,[row[0]]+list(map(int,row[1:])))) for row in rows}
    if len(observed)!=len(rows) or set(observed)!={c['id'] for c in tests}:
        raise ValueError('Duplicate/missing actual Max RGB cases')
    event_rows=table(files,'events.tsv',6)
    merge_rows=table(files,'merges.tsv',7)
    merge_context={r[0]:tuple(map(int,r[1:])) for r in merge_rows}
    if len(merge_context)!=len(merge_rows): raise ValueError('Repeated merge context row')
    roi_rows=table(files,'rois.tsv',7)
    rois={(r[0],int(r[1])):tuple(map(int,r[2:])) for r in roi_rows}
    if len(rois)!=len(roi_rows): raise ValueError('Repeated captured run ROI')
    statuses=table(files,'statuses.tsv',3)
    status_map={r[0]:r[1:] for r in statuses}
    if len(status_map)!=len(statuses) or set(status_map)!={t['id'] for t in tests if not t['live']}:
        raise ValueError('Missing exact synchronous PDB status observations')
    if any(status!=['3',''] for status in status_map.values()):
        raise ValueError('RGB/RGBA PDB execution failed')
    records=[]; shapes=[]
    for test in tests:
        ident=test['id']; row=observed[ident]
        if test['components'] not in (3,4): raise ValueError('Unsafe non-RGB oracle domain')
        c=4 if test['live'] else test['components']
        rerun=bool(test['rerun']); runs=1+rerun if test['live'] else 0
        final_kind=test['final_selection'] if test['final_selection']>=0 else test['selection']
        idle_kind=test['idle_selection'] if test['idle_selection']>=0 else final_kind
        merges=int(test['selection']!=3)+int(rerun and idle_kind!=3)
        status=-1 if test['live'] else 3
        if (row['starts'],row['ends'],row['merges'],row['status'])!=(runs,runs,merges,status):
            raise ValueError('Wrong actual run/merge/rejection counts '+ident)
        if row['output_channels']!=c or not -2147483648<=test['mode']<=2147483647:
            raise ValueError('Invalid captured layout or signed mode '+ident)
        for observed_key,case_key in (('width','width'),('height','height'),('mode','mode'),
            ('live','live'),('image_width','image_width'),('image_height','image_height'),
            ('offset_x','offset_x'),('offset_y','offset_y'),('writer','writer'),
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
        check_phase(events,test['phase'],row['mutated'],ident)
        if test['phase']==2 and test['selection'] in (5,6):
            p=next(float(e[4]) for e in events if e[1]=='progress-mid-mutate')
            if p!=0.25: raise ValueError('Tile-crossing progress is not observed .25 '+ident)
        for n in range(1,runs+1):
            if sum(r[1]=='start' and int(r[2])==n for r in events)!=1 or sum(
                r[1]=='end' and int(r[3])==n for r in events)!=1:
                raise ValueError('Missing exact start/end observation '+ident)
            start_index=next(i for i,e in enumerate(events) if e[1]=='start' and int(e[2])==n)
            end_index=next(i for i,e in enumerate(events) if e[1]=='end' and int(e[3])==n)
            if start_index>=end_index: raise ValueError('Actual end did not follow start '+ident)
            expected_merges=0 if (test['selection'] if n==1 else idle_kind)==3 else 1
            local=events[start_index+1:end_index]
            before=[i for i,e in enumerate(local) if e[1]=='merge-before']
            after=[i for i,e in enumerate(local) if e[1]=='merge-after']
            if len(before)!=expected_merges or len(after)!=expected_merges or (before and before[0]>=after[0]):
                raise ValueError('Merge observers are not ordered inside the actual run '+ident)
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
        if test['live'] and pixels('input')!=pixels('start1'):
            raise ValueError('First actual run input/start disagreement '+ident)
        if test['selection']==3 and pixels('input')!=pixels('output'):
            raise ValueError('No-merge invocation changed drawable '+ident)
        if rerun and idle_kind==3 and pixels('start2')!=pixels('output2'):
            raise ValueError('No-merge second run changed drawable '+ident)
        if test['rerun']==1 and pixels('source')==pixels('source2'):
            raise ValueError('Changed-source control did not change bytes '+ident)
        if test['rerun']==2 and pixels('source')!=pixels('source2'):
            raise ValueError('Unchanged-source notification altered pixels '+ident)
        if idle and pixels('idle-output')!=pixels('output'):
            raise ValueError('Idle context edit altered settled cache '+ident)
        hidden_channels=0; raw_pixels=0; tied_pixels=0; hidden_raw_changes=0
        for n in range(1,(1+rerun)+1):
            selected,x1,y1,x2,y2=rois[ident,n]
            mask=pixels('start-mask' if n==1 else 'start2-mask')
            if (selected,x1,y1,x2,y2)!=mask_roi(mask,test['image_width'],test['image_height'],
                    test['offset_x'],test['offset_y'],test['width'],test['height']):
                raise ValueError('Actual ROI differs from captured start mask '+ident)
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
            effective_bits=bits & ~(1 << (c-1)) if lock and c==4 else bits
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

            hidden_channels+=check_merge(inp,shadow,coverage,output,c,effective_bits)
            width=test['width']
            for offset in range(0,len(inp),c):
                index=offset//c; x=index%width; y=index//width
                in_roi=x1<=x<x2 and y1<=y<y2
                original=inp[offset:offset+c]
                expected=raw_pixel(original,test['mode']) if in_roi else bytes(c)
                if shadow[offset:offset+c]!=expected:
                    raise ValueError('Actual shadow differs from start ROI/point equation '+ident+' '+str(n)+' '+str(index))
                if in_roi:
                    raw_pixels+=1
                    chosen=(max if test['mode']>0 else min)(original[:3])
                    tied_pixels+=int(sum(v==chosen for v in original[:3])>1)
                    hidden_raw_changes+=int(c==4 and original[3]==0 and expected[:3]!=original[:3])
        record=dict(test,observed=row,buffers=buffers,raw_proof=dict(
            raw_pixels_checked=raw_pixels,chosen_channel_tie_pixels=tied_pixels,
            hidden_rgb_raw_changed_pixels=hidden_raw_changes),
            zero_alpha_hidden_color_channels=hidden_channels)
        if test['writer']:
            shape=inspect_writer(files,test); record['writer']=shape; shapes.append(shape)
        records.append(record)
    return records,shapes


def validate_directory(out, tests, code):
    out=Path(out); errors=[]; records=[]; shapes=[]
    files={p.name:p.read_bytes() for p in out.iterdir() if p.is_file()}
    if code: errors.append('Harness exit '+str(code))
    try: check_runtime_diagnostics(files.get('capture.log',b''))
    except ValueError as exc: errors.append(str(exc))
    if b'LEGACY_MAX_RGB_CAPTURE_COMPLETE' not in files.get('capture.log',b''):
        errors.append('Missing successful completion marker')
    # Extract the actual original capsules first, then independently decode all
    # framing, tags, lengths, signed payloads and terminal/tail boundaries.
    from capture_legacy_max_rgb import writer_shape
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
        phase_kinds={'0':'none','1':'actual core merge entry after completed shadow flush','2':'mid_work: actual 0<progress<1 before merge'},
        rerun_kinds={'0':'none','1':'changed lower-source bytes','2':'unchanged-byte lower-source notification'},
        roi_columns='id run selected x1 y1 x2 y2'.split(),
        event_columns='id event starts ends progress merge_count'.split(),
        layout_policy='RGB/RGBA only; original noninteractive callback is unsafe for Gray and is never probed. Live RGB input promotes to RGBA.',
        status_policy='PDB status is observed; async live -1 is unavailable, with exact start/end/merge evidence.',
        mask_policy='start/final/idle masks image-sized; merge masks drawable-local effective coverage.',
        diagnostics_policy='Inherited legacy diagnostics retained in capture report; no warning-free runtime claim.')
    (out/'fixtures.json').write_text(json.dumps(fixture,indent=2)+'\n')
    (out/'writer-shapes.json').write_text(json.dumps(shapes,indent=2)+'\n')
    return errors,len(records)


def read_archive(path,prefix,archive_hash,manifest_hash,byte_limit,member_limit,dictionary_bytes):
    path=Path(path)
    if not archive_hash or not manifest_hash: raise ValueError('Max RGB capture has not yet been sealed')
    if path.is_symlink(): raise ValueError('Symlink evidence archive')
    raw=path.read_bytes(); manifest_raw=path.with_suffix('.manifest.json').read_bytes()
    if len(raw)>8*1024*1024 or sha(raw)!=archive_hash or sha(manifest_raw)!=manifest_hash:
        raise ValueError('Changed or unbounded Max RGB archive/manifest')
    manifest=json.loads(manifest_raw)
    if (manifest['schema_version']!=1 or manifest['compression']!='xz-lzma2' or
        manifest['archive_sha256']!=archive_hash or manifest['dictionary_bytes']!=dictionary_bytes or
        manifest['member_count']!=len(manifest['members']) or manifest['member_count']>member_limit or
        manifest['total_bytes']>byte_limit): raise ValueError('Invalid Max RGB archive manifest')
    files={}; total=0
    with tarfile.open(path,'r:xz') as archive:
        for member in archive:
            name=PurePosixPath(member.name)
            if (not member.isfile() or len(name.parts)!=2 or name.parts[0]!=prefix or
                not safe_name(name.name) or name.name in files or name.name not in manifest['members'] or
                member.size>8*1024*1024 or total+member.size>byte_limit):
                raise ValueError('Unsafe Max RGB archive member')
            data=archive.extractfile(member).read(member.size+1); entry=manifest['members'][name.name]
            if len(data)!=member.size or len(data)!=entry['size'] or sha(data)!=entry['sha256']:
                raise ValueError('Changed Max RGB archive member')
            files[name.name]=data; total+=len(data)
    if set(files)!=set(manifest['members']) or total!=manifest['total_bytes']:
        raise ValueError('Incomplete Max RGB corpus')
    return files


def check_capture(files,report_hash,large):
    if not report_hash or sha(files['capture-report.json'])!=report_hash:
        raise ValueError('Changed sealed Max RGB report')
    report=json.loads(files['capture-report.json'])
    if (report['status']!='passed' or report['errors'] or report['exit_code']!=0 or
        report['source_commit']!=COMMIT or report['large_cases']!=large or
        report['source_sha256']!=report['post_capture_source_sha256'] or
        report['executable_sha256']!=report['post_capture_executable_sha256']):
        raise ValueError('Invalid Max RGB capture provenance')
    if set(files)!=set(report['files_sha256'])|{'capture-report.json'}:
        raise ValueError('Unrecorded or missing Max RGB artifact')
    for name,digest in report['files_sha256'].items():
        if not safe_name(name) or sha(files[name])!=digest: raise ValueError('Changed Max RGB artifact '+name)
    for name,key in (('capture-script.py','capture_script_sha256'),('capture-harness.c','harness_sha256'),
                     ('observation-hooks.patch','instrumentation_sha256'),('observation-helper.py','observation_helper_sha256'),
                     ('evidence-validator.py','evidence_validator_sha256')):
        if sha(files[name])!=report[key]: raise ValueError('Changed Max RGB capture source '+name)
    if b'LEGACY_MAX_RGB_CAPTURE_COMPLETE' not in gzip.decompress(files['capture.log.gz']):
        raise ValueError('No successful genuine capture completion')
    check_runtime_diagnostics(gzip.decompress(files['capture.log.gz']))
    tests=json.loads(files['capture-plan.json'])
    from capture_legacy_max_rgb import cases as planned_cases
    if tests!=planned_cases(large): raise ValueError('Sealed capture does not match complete deterministic matrix')
    expected=(2,2,0) if large else (218,124,14)
    if (len(tests),sum(bool(c['live']) for c in tests),sum(bool(c['writer']) for c in tests))!=expected:
        raise ValueError('Incomplete bounded Max RGB coverage matrix')
    if report['case_count']!=len(tests) or report['expected_cases']!=len(tests):
        raise ValueError('Wrong Max RGB capture counts')
    records,shapes=observations(files,tests)
    fixture=json.loads(files['fixtures.json'])
    if fixture['capture_columns']!=COLUMNS or records!=fixture['cases'] or shapes!=json.loads(files['writer-shapes.json']):
        raise ValueError('Max RGB captured observations and indexes disagree')
    return report,records


def verify(archive=DEFAULT,extract=None):
    base=read_archive(archive,'legacy-max-rgb-context',BASE_ARCHIVE_SHA256,BASE_MANIFEST_SHA256,
                      40*1024*1024,5000,8*1024*1024)
    base_report,base_cases=check_capture(base,BASE_REPORT_SHA256,False)
    large=read_archive(LARGE,'legacy-max-rgb-large',LARGE_ARCHIVE_SHA256,LARGE_MANIFEST_SHA256,
                       120*1024*1024,100,32*1024*1024)
    large_report,large_cases=check_capture(large,LARGE_REPORT_SHA256,True)
    if base_report['source_sha256']!=large_report['source_sha256']:
        raise ValueError('Max RGB corpora disagree about pinned sources')
    if {c['id'] for c in base_cases}&{c['id'] for c in large_cases}: raise ValueError('Duplicate capture ID')
    if extract is not None:
        target=Path(extract)
        if target.is_symlink() or (target.exists() and any(target.iterdir())):
            raise ValueError('Max RGB target must be absent or empty')
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

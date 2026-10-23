#!/usr/bin/env python3
"""Verify the separate genuine Sobel projection-history supplement."""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path
from check_sobel_evidence import read_archive, check_shadow, check_runtime_diagnostics, check_merge, check_signature
ROOT=Path(__file__).resolve().parents[1]
COMMIT='afa43fae3e920210146abed514f136fd49f671b5'
DEFAULT=ROOT/'migration/fixtures/legacy-sobel-projection.tar.xz'
ARCHIVE_SHA256='8abd3b57789d5a2e8777e3c8f402545521ed2f87b95ffb5200c9d27819ad6d26'
MANIFEST_SHA256='c310b47776fe380ab2003c4a72021099f4a1783a9d0f09a0ac6e60811bf07e12'
REPORT_SHA256='e14788d46622dfc60dbb981b9248fcfaefa608d28cbe4410607c33e4e9bef11b'
COLUMNS=('id source_channels grouped seeded initial_x initial_y final_x final_y '
         'starts ends merges copies initial_loaded_starts initial_loaded_ends').split()
COPY_COLUMNS=('id run channels width height filter_x filter_y parent_x parent_y parent_width parent_height '
              'source_x source_y dest_x dest_y copy_width copy_height starts ends merges').split()

def sha(raw): return hashlib.sha256(raw).hexdigest()
def table(files,name,columns):
    rows=[x.split('\t') for x in files[name].decode().splitlines()]
    if any(len(x)!=columns for x in rows): raise ValueError('Invalid table '+name)
    return rows

def pattern(case,variant):
    c=case['components'];w=case['width'];h=case['height'];alpha=(0,1,126,127,128,254,255,63)
    return bytes(alpha[(x+3*y+variant*3)%8] if channel==c-1 else
                 (x*(37+channel*17)+y*(61+channel*23)+variant*97+channel*51+11)%256
                 for y in range(h) for x in range(w) for channel in range(c))

def observations(files,tests):
    check_signature(files)
    rows=table(files,'captures.tsv',14);copies=table(files,'copies.tsv',20)
    row_map={r[0]:r for r in rows};copy_map={(r[0],int(r[1])):r for r in copies}
    if len(row_map)!=len(rows) or set(row_map)!={t['id'] for t in tests} or len(copy_map)!=3*len(tests):
        raise ValueError('Incomplete projection-history matrix')
    events=table(files,'events.tsv',6);rois=table(files,'rois.tsv',7);merges=table(files,'merges.tsv',7)
    options=table(files,'options.tsv',5)
    option_map={(r[0],int(r[1])):list(map(int,r[2:])) for r in options}
    if len(option_map)!=len(options) or len(option_map)!=3*len(tests): raise ValueError('Incomplete observed flag history')
    roi_map={(r[0],int(r[1])):list(map(int,r[2:])) for r in rois}
    merge_map={r[0]:list(map(int,r[1:])) for r in merges}
    records=[];preserved_pixels=0;group_offcanvas_pixels=0;halo_pixels=0;halo_dependent_runs=0;hidden_color_channels=0
    for t in tests:
        ident=t['id'];c=t['components'];w=t['width'];h=t['height'];size=w*h*c
        final_x,final_y=(-4,-3) if t['offset_x']>=0 else (3,2)
        expected=[c,t['grouped'],t['seeded'],t['offset_x'],t['offset_y'],final_x,final_y,3,3,3,3,
                  0 if t['seeded'] else -1,0 if t['seeded'] else -1]
        if list(map(int,row_map[ident][1:]))!=expected: raise ValueError('Wrong projection counts/context '+ident)
        buffers={}
        def take(label,length=size):
            name=ident+'-'+label+'.raw';raw=files[name]
            if len(raw)!=length: raise ValueError('Wrong projection buffer extent '+name)
            buffers[label]=dict(file=name,bytes=length,sha256=sha(raw));return raw
        seed=take('seed');source=take('source');take('input')
        if source!=pattern(t,0) or seed!=(pattern(t,2) if t['seeded'] else bytes(size)):
            raise ValueError('Non-synthetic seed/source '+ident)
        mask=take('start-mask',t['image_width']*t['image_height'])
        if any(mask): raise ValueError('Projection supplement selection is not empty '+ident)
        if t['seeded'] and take('loaded-cache')!=seed: raise ValueError('Loaded initial cache changed '+ident)
        previous=seed;runs=[]
        for run,flags in ((1,[0,0,1]),(2,[1,1,0]),(3,[1,1,0])):
            if option_map[ident,run]!=flags: raise ValueError('Wrong history flag values')
            row=copy_map[ident,run];q=dict(zip(COPY_COLUMNS,[row[0]]+list(map(int,row[1:]))))
            x,y=(t['offset_x'],t['offset_y']) if run<3 else (final_x,final_y)
            if [q[k] for k in ('channels','width','height','filter_x','filter_y','starts','ends','merges')]!=[c,w,h,x,y,run-1,run-1,run-1]:
                raise ValueError('Wrong actual copy phase or drawable geometry '+ident)
            if t['grouped']:
                px=min(x,t['offset_x']);py=min(y,t['offset_y'])
                pw=max(x,t['offset_x'])+w-px;ph=max(y,t['offset_y'])+h-py
            else: px=py=0;pw=t['image_width'];ph=t['image_height']
            if [q[k] for k in ('parent_x','parent_y','parent_width','parent_height')]!=[px,py,pw,ph]:
                raise ValueError('Observed ordinary parent projection bounds differ '+ident)
            dx=max(0,px-x);dy=max(0,py-y);sx=max(0,x-px);sy=max(0,y-py)
            cw=min(w-dx,pw-sx);ch=min(h-dy,ph-sy)
            if [q[k] for k in ('source_x','source_y','dest_x','dest_y','copy_width','copy_height')]!=[sx,sy,dx,dy,cw,ch]:
                raise ValueError('Observed copy does not match parent intersection '+ident)
            before=take(f'before{run}');projection=take(f'projection{run}',pw*ph*c)
            start=take(f'start{run}');start_mask=take(f'start{run}-mask',t['image_width']*t['image_height'])
            selected=bool(t['halo'] and run>=2)
            expected_mask=bytes([255 if selected else 0])*(t['image_width']*t['image_height'])
            if start_mask!=expected_mask or before!=previous: raise ValueError('Prior cache/mask history differs '+ident)
            copied=bytearray(before)
            for yy in range(ch):
                dest=((dy+yy)*w+dx)*c;src=((sy+yy)*pw+sx)*c
                copied[dest:dest+cw*c]=projection[src:src+cw*c]
            if start!=copied: raise ValueError('Actual parent copy did not preserve untouched cache '+ident)
            if run==1 and start!=files[ident+'-input.raw']: raise ValueError('First input/start disagreement')
            for yy in range(h):
                for xx in range(w):
                    at=(yy*w+xx)*c
                    if not (dx<=xx<dx+cw and dy<=yy<dy+ch):
                        if start[at:at+c]!=previous[at:at+c]: raise ValueError('Outside projection cache lost')
                        # Only run 2 follows both-disabled/keep-sign result at unchanged bounds.
                        # Run 3 can uncover pixels that run 2 changed; their
                        # exact prior-cache equality is checked above.
                        if run==2:
                            if start[at+c-1]!=255: raise ValueError('Prior both-disabled/keep-sign alpha was not preserved')
                            preserved_pixels+=1
                    if t['grouped'] and not(0<=x+xx<t['image_width'] and 0<=y+yy<t['image_height']):
                        group_offcanvas_pixels+=1
            if take(f'source{run}')!=source: raise ValueError('Unchanged lower notification changed source '+ident)
            x1,y1,x2,y2=(max(0,-x),max(0,-y),min(w,t['image_width']-x),min(h,t['image_height']-y)) if selected else (0,0,w,h)
            if roi_map[ident,run]!=[int(selected),x1,y1,x2,y2]: raise ValueError('Unexpected start selection bounds '+ident)
            if merge_map[ident+f'-merge{run}']!=[w,h,c,(1<<c)-1,int(not selected),0]: raise ValueError('Unexpected final merge context '+ident)
            merge_input=take(f'merge{run}-input');shadow=take(f'merge{run}-shadow');coverage=take(f'merge{run}-mask',w*h)
            output=take(f'output{run}');merged=take(f'merge{run}-output')
            expected_coverage=bytes(255 if not selected or (x1<=xx<x2 and y1<=yy<y2) else 0
                                    for yy in range(h) for xx in range(w))
            if merge_input!=start or coverage!=expected_coverage: raise ValueError('Unexpected actual merge input/mask '+ident)
            check_shadow(start,shadow,w,h,c,(x1,y1,x2,y2),flags)
            hidden_color_channels+=check_merge(start,shadow,coverage,output,c,(1<<c)-1)
            if merged!=output: raise ValueError('Final drawable differs from captured merge output '+ident)
            if selected:
                without_retained_halo=bytearray(start)
                for yy in (y1-1,y2):
                    if 0<=yy<h and not dy<=yy<dy+ch:
                        for xx in range(x1,x2):
                            at=(yy*w+xx)*c
                            if start[at:at+c]!=previous[at:at+c]: raise ValueError('Outside-ROI vertical halo lost cache history')
                            without_retained_halo[at:at+c]=bytes(c)
                            halo_pixels+=1
                try:
                    check_shadow(without_retained_halo,shadow,w,h,c,(x1,y1,x2,y2),flags)
                except ValueError:
                    halo_dependent_runs+=1
                else:
                    raise ValueError('Retained vertical halo case does not distinguish zero-prefill '+ident)
            local=[e for e in events if e[0]==ident]
            for event,key,value in (('projection-copy',2,run-1),('start',2,run),('merge-before',5,run),('merge-after',5,run),('end',3,run)):
                if sum(e[1]==event and int(e[key])==value for e in local)!=1: raise ValueError('Missing exact phase event '+ident)
            before_index=next(i for i,e in enumerate(local) if e[1]=='projection-copy' and int(e[2])==run-1)
            start_index=next(i for i,e in enumerate(local) if e[1]=='start' and int(e[2])==run)
            if before_index>=start_index: raise ValueError('Copy observation did not precede run')
            runs.append(dict(run=run,horizontal=flags[0],vertical=flags[1],keep_sign=flags[2],copy=q));previous=output
        records.append(dict(t,observed=dict(zip(COLUMNS,[row_map[ident][0]]+list(map(int,row_map[ident][1:])))),runs=runs,buffers=buffers))
    if not preserved_pixels or not group_offcanvas_pixels or not halo_pixels or halo_dependent_runs!=16:
        raise ValueError('Supplement lacks all16 enabled-gradient retained-halo witnesses')
    return records,dict(preserved_after_first_run_pixels=preserved_pixels,group_outside_image_pixels=group_offcanvas_pixels,vertical_halo_retained_cache_pixels=halo_pixels,distinguishing_retained_halo_runs=halo_dependent_runs,zero_alpha_hidden_color_channels=hidden_color_channels)

def validate_directory(out,tests,code):
    out=Path(out);files={p.name:p.read_bytes() for p in out.iterdir() if p.is_file()};errors=[];records=[];proof={}
    if code: errors.append('Harness exit '+str(code))
    try: check_runtime_diagnostics(files.get('capture.log',b''))
    except ValueError as exc: errors.append(str(exc))
    if b'LEGACY_SOBEL_PROJECTION_CAPTURE_COMPLETE' not in files.get('capture.log',b''):
        errors.append('No successful projection capture completion')
    try: records,proof=observations(files,tests)
    except (KeyError,ValueError) as failure: errors.append(str(failure))
    (out/'fixtures.json').write_text(json.dumps(dict(schema_version=1,capture_columns=COLUMNS,copy_columns=COPY_COLUMNS,
        cases=records,proof=proof,loaded_policy='Synthetic seeded pixels use the actual mark_as_loaded API; no actual XCF reopening is claimed.',
        group_policy='Ordinary automatic group bounds, measured by pre-copy observer; no forced clipping/resize.',
        source_policy='All source/sourceN buffers are unchanged deterministic synthetic lower pixels.'),indent=2)+'\n')
    return errors,len(records)

def check_capture(files,report_hash):
    if not report_hash or sha(files['capture-report.json'])!=report_hash: raise ValueError('Changed projection report')
    r=json.loads(files['capture-report.json'])
    if (r['status']!='passed' or r['errors'] or r['exit_code']!=0 or r['source_commit']!=COMMIT or
        r['case_count']!=24 or r['expected_cases']!=24 or r['source_sha256']!=r['post_capture_source_sha256'] or
        r['executable_sha256']!=r['post_capture_executable_sha256']): raise ValueError('Invalid projection provenance')
    if set(files)!=set(r['files_sha256'])|{'capture-report.json'}: raise ValueError('Unexpected projection artifacts')
    for name,digest in r['files_sha256'].items():
        if sha(files[name])!=digest: raise ValueError('Changed projection artifact '+name)
    for name,key in (('capture-script.py','capture_script_sha256'),('capture-harness.c','harness_sha256'),
        ('observation-hooks.patch','instrumentation_sha256'),('observation-helper.py','observation_helper_sha256'),
        ('evidence-validator.py','evidence_validator_sha256'),
        ('base-evidence-validator.py','base_evidence_validator_sha256')):
        if sha(files[name])!=r[key]: raise ValueError('Changed projection capture source')
    if b'LEGACY_SOBEL_PROJECTION_CAPTURE_COMPLETE' not in gzip.decompress(files['capture.log.gz']):
        raise ValueError('Missing successful capture log marker')
    if check_runtime_diagnostics(gzip.decompress(files['capture.log.gz']))!=r['runtime_diagnostics']:
        raise ValueError('Projection runtime diagnostics were not retained exactly')
    tests=json.loads(files['capture-plan.json']);records,proof=observations(files,tests)
    manifest=json.loads(files['fixtures.json'])
    if records!=manifest['cases'] or proof!=manifest['proof'] or len(records)!=24: raise ValueError('Projection index differs')
    return r,records,proof


def verify(archive=DEFAULT,extract=None):
    files=read_archive(archive,'legacy-sobel-projection',ARCHIVE_SHA256,MANIFEST_SHA256,8*1024*1024,1400,8*1024*1024)
    report,records,proof=check_capture(files,REPORT_SHA256)
    if extract is not None:
        target=Path(extract)
        if target.is_symlink() or (target.exists() and any(target.iterdir())): raise ValueError('Nonempty projection target')
        target.mkdir(parents=True,exist_ok=True)
        for name,raw in files.items(): (target/name).write_bytes(raw)
    return dict(source_commit=COMMIT,case_count=24,live_runs=72,source_layouts=[2,4],report_sha256=REPORT_SHA256,**proof)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--extract',type=Path);a=p.parse_args()
    print(json.dumps(verify(extract=a.extract),sort_keys=True))

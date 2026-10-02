#!/usr/bin/env python3
"""Capture exact original PDB outputs; no ported-kernel oracle or XCF reader."""
import argparse, hashlib, json, os, shutil, subprocess
from datetime import datetime, timezone
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--capture',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'migration/fixtures/legacy-filter-points');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
if (out/'capture-report.json').exists():raise SystemExit('Sealed report exists; use a new output directory')
prefix=Path('/workspace/shared/gimp-legacy-build/prefix');legacy=ROOT.parent/'gimp-painter-legacy'
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
assert subprocess.check_output(['git','-C',str(legacy),'rev-parse','HEAD'],text=True).strip()=='afa43fae3e920210146abed514f136fd49f671b5'
exe=prefix/'bin/gimp-2.8';assert sha(exe)=='fefc8fa5190592780b5bd880226a99344268b07074b294431903583e9b5013c8'
inputs=[];cases=[]
for mode,w,h in [('RGBA',9,8),('RGBA',1,1),('RGBA',67,66),('RGB',9,8),('LA',9,8)]:
    name=f'{mode}-{w}x{h}';channels=len(mode)
    data=bytes(v for y in range(h) for x in range(w) for v in
        (((x*37+y*61+11)%256, (0,1,63,127,128,254,255)[(x+3*y)%7]) if mode=='LA' else
         ((x*37+y*61+11)%256,(x*97+y*43+61)%256,(x*13+y*173+137)%256)+
         (((0,1,63,127,128,254,255)[(x+3*y)%7],) if mode=='RGBA' else ())))
    (out/(name+'.raw')).write_bytes(data);Image.frombytes(mode,(w,h),data).save(out/(name+'.png'))
    inputs.append(dict(name=name,mode=mode,width=w,height=h,channels=channels))
    ops=[]
    if mode!='LA':ops += [('plug-in-vinvert',0)] + [('plug-in-max-rgb',v) for v in (-2,0,1,3)]
    if mode in ('LA','RGBA'):ops += [('plug-in-threshold-alpha',v) for v in (-1,0,127,255,256)]
    for proc,arg in ops:
        ident=f'{name}-{proc}-{arg}';cases.append(dict(id=ident,procedure=proc,argument=arg,width=w,height=h,
            channels=channels,input=name+'.raw',output=ident+'.raw',mode=mode))
q=lambda x:json.dumps(str(x))
def load(name):
    f=q(out/name);return f'(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE {f} {f}))) (layer (car (gimp-image-get-active-layer image))))'
def save(name):
    f=q(out/name);return f'(file-png-save2 RUN-NONINTERACTIVE image layer {f} {f} 0 9 0 0 0 0 0 0 1)'
script=[]
for src in inputs:script.append(load(src['name']+'.png')+' '+save('loaded-'+src['name']+'.png')+' (gimp-image-delete image))')
for c in cases:
    script.append(load(Path(c['input']).with_suffix('.png').name)+f" ({c['procedure']} RUN-NONINTERACTIVE image layer"+
        ('' if c['procedure']=='plug-in-vinvert' else ' '+str(c['argument']))+') '+save(c['id']+'.png')+
        f' (gimp-message "POINT_DONE={c["id"]}") (gimp-image-delete image))')
(out/'capture.scm').write_text('\n'.join(script)+'\n')
(out/'fixtures.tsv').write_text('# procedure argument width height channels input output\n'+''.join(
    '\t'.join(str(c[k]) for k in ('procedure','argument','width','height','channels','input','output'))+'\n' for c in cases))
(out/'.gitattributes').write_text('*.raw binary\n')
if not a.capture:print(len(cases));raise SystemExit(0)
home=prefix.parent/'runtime-filter-points';profile=home/'profile';profile.mkdir(parents=True,exist_ok=True)
shutil.copyfile(prefix.parent/'runtime-home/profile/pluginrc',profile/'pluginrc')
env=dict(os.environ);env.update(HOME=str(home),GIMP2_DIRECTORY=str(profile))
cmd=[str(exe),'--no-interface','--no-data','--no-fonts','--no-splash','--new-instance','--batch-interpreter=plug-in-script-fu-eval',
     '-b','(load '+q(out/'capture.scm')+')','-b','(gimp-quit 0)']
with (out/'capture.log').open('w') as f: result=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
log=(out/'capture.log').read_text(errors='replace');errors=[]
for src in inputs:
    image=Image.open(out/('loaded-'+src['name']+'.png'))
    if image.mode!=src['mode'] or image.tobytes()!=(out/(src['name']+'.raw')).read_bytes():errors.append('Input conversion '+src['name'])
for c in cases:
    path=out/(c['id']+'.png')
    if not path.exists() or 'POINT_DONE='+c['id'] not in log:errors.append('Missing '+c['id']);continue
    image=Image.open(path)
    if image.mode!=c['mode']:errors.append('Output mode '+c['id']);continue
    (out/c['output']).write_bytes(image.tobytes());c['output_sha256']=sha(out/c['output'])
report=dict(schema_version=1,captured_utc=datetime.now(timezone.utc).isoformat(),source_commit='afa43fae3e920210146abed514f136fd49f671b5',
    executables={str(f.relative_to(prefix)):sha(f) for f in [exe]+[prefix/'lib/gimp/2.0/plug-ins'/s for s in ('value-invert','max-rgb','threshold-alpha','file-png')]},
    source_sha256={n:sha(legacy/'plug-ins/common'/n) for n in ('value-invert.c','max-rgb.c','threshold-alpha.c')},
    command=cmd,inputs=inputs,cases=cases,case_count=len(cases),errors=errors,exit_code=result.returncode,
    status='passed' if not errors and result.returncode==0 else 'failed',
    script_sha256=sha(out/'capture.scm'),log_sha256=sha(out/'capture.log'),fixtures_sha256=sha(out/'fixtures.tsv'))
(out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'],len(cases),errors)
raise SystemExit(0 if report['status']=='passed' else 1)

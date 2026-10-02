from pathlib import Path
from PIL import Image
import json
p=Path('/workspace/shared/legacy-normal-multiply-capture');p.mkdir(exist_ok=True)
a=[0,1,64,127,128,192,254,255]
for which in ['backdrop','source']:
 b=bytearray()
 for y in range(8):
  for x in range(8):
   b.extend(((x*37+y*17+(31 if which=="backdrop" else 191))%256,(x*23+y*53+(13 if which=="backdrop" else 89))%256,(x*91+y*29+(77 if which=="backdrop" else 7))%256,a[x if which=='backdrop' else y]))
 (p/(which+'.rgba')).write_bytes(b);Image.frombytes('RGBA',(8,8),bytes(b)).save(p/(which+'.png'))
scm=[];cases=[]
for mode in [0,3]:
 for opacity,mask in [(100,None),(50,None),(0,None),(100,128),(50,128),(100,0),(1,254),(99,1)]:
  ident=f'mode-{mode}-o{opacity}-m{mask if mask is not None else "none"}'
  out=p/(ident+'.png')
  s=f'''(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "{p}/backdrop.png" "{p}/backdrop.png")))
 (top (car (gimp-file-load-layer RUN-NONINTERACTIVE image "{p}/source.png"))))
 (gimp-image-insert-layer image top 0 0)
 (gimp-layer-set-mode top {mode})
 (gimp-layer-set-opacity top {opacity})
'''
  if mask is not None:s+=f''' (let ((mask (car (gimp-layer-create-mask top ADD-WHITE-MASK))))
 (gimp-layer-add-mask top mask)
 (gimp-context-set-foreground '({mask} {mask} {mask}))
 (gimp-edit-fill mask FOREGROUND-FILL))
'''
  s+=f''' (let ((visible (car (gimp-layer-new-from-visible image image "projection"))))
 (gimp-image-insert-layer image visible 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image visible "{p}/{ident}-projection.png" "{p}/{ident}-projection.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-remove-layer image visible))
 (let ((merged (car (gimp-image-merge-visible-layers image CLIP-TO-IMAGE))))
 (file-png-save2 RUN-NONINTERACTIVE image merged "{out}" "{out}" 0 9 0 0 0 0 0 0 1))
 (gimp-image-delete image)
 (gimp-message "MODE_CASE_DONE={ident}"))
'''
  scm.append(s);cases.append(dict(id=ident,mode=mode,opacity=opacity,mask=mask))
(p/'capture.scm').write_text('; Real old layer compositor through ordinary PDB merge-visible-layers\n'+'\n'.join(scm))
(p/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')

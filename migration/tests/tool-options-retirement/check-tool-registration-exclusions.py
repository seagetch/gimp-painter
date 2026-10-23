import pathlib,subprocess,hashlib,json
r=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');w=pathlib.Path('/workspace/shared/gimp-native-preset-gui'); rows=[]
for p in ['app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/tests/test-gimp-painter-mybrush.c','app/tests/test-painter-mybrush-duplicate-handle.cpp']:
 old=subprocess.check_output(['git','show','77bcdf2b7f:'+p],cwd=r);current=(w/'source'/p).read_bytes();assert current==old,p;rows.append({'path':p,'sha256':hashlib.sha256(current).hexdigest(),'baseline':'77bcdf2b7f'})
(w/'tool-registration-evidence/final-exclusion-check.json').write_text(json.dumps(rows,indent=2)+'\n')

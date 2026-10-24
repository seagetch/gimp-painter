from pathlib import Path
import json,subprocess,difflib,importlib.util,unittest.mock,hashlib
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');S=Path('/workspace/shared/gimp-clean-aggregate/source');O=Path('/workspace/shared/editor-header-routes');path='migration/inventory/additional-c-header-routes.json';base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip();old=subprocess.check_output(['git','show',base+':'+path],cwd=R,text=True);j=json.loads(old)
rows=[{'path':'app/actions/mypaint-brush-editor-actions.h','kind':'new','context':'core','prerequisites':[],'review':'24.015/named-native-editor-entry: native C action setup/update declarations after reviewed core/widget type prelude; first/double C11/C++14 compile. C-only callbacks, no cross-language link ABI or historical DataEditor action-layout claim.'},{'path':'app/widgets/gimpmypaintbrusheditor.h','kind':'new','context':'standalone','prerequisites':[],'review':'24.015/native-editor-type-alias: opaque alias of sole native owner with exact named constructor/getter, self-owned GTK/core/widget prerequisites and C linkage. First/double C11/C++14 compile; no old GimpDataEditor instance/class/override ABI.'}]
for row in rows:assert row['path'] not in [x['path'] for x in j['routes']]
j['routes'].extend(rows);new=json.dumps(j,indent=2)+'\n';(O/'routes.patch').write_text(''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/'+path,tofile='b/'+path)));(O/'candidate.json').write_text(new)
spec=importlib.util.spec_from_file_location('probe',S/'app/painter/tests/headers/generate-probes.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);original=Path.read_text
def run(text):
 def read(p,*a,**k):return text if p==S/path else original(p,*a,**k)
 with unittest.mock.patch.object(Path,'read_text',read):return m.specification(S,False)
probes,manifest=run(new);(O/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');controls=[]
for row in rows:
 broken=json.loads(new);broken['routes']=[x for x in broken['routes'] if x['path']!=row['path']]
 try:run(json.dumps(broken))
 except ValueError as e:assert row['path'] in str(e);controls.append(str(e))
assert len(controls)==2
(O/'receipt.json').write_text(json.dumps({'base':base,'source_root':str(S),'scope_overlay_only':path,'rows':rows,'route_negative_controls':controls,'frozen_input_sha256':manifest['source_sha256'],'probe_count':len(probes)},indent=2)+'\n');print('PASS generator',len(probes),'negative controls',controls)

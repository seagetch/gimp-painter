#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,shutil,subprocess,csv,re
b=Path(__file__).resolve().parent;d=b/'deliverables/migration/fixtures/legacy-rotation-operations';d.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['README.md','STAGING.md','RECONSTRUCTION.md','run_gui_fresh.sh','guard-validation.log','capture-rotation.c','build.sh','run_gui.sh','run_authenticated_display.py','prepare_source_capture.py','source-driver.c','source-capture.c','replay_source.sh','test_rotation_capture.py','collect-provenance.py','package.py','source-provenance.json','source-overlay.patch','archive-provenance.json','dependencies.txt','binary.sha256','source-binaries.sha256','original-binary-after.sha256','build.log','validation.log','patch-check.log']:
 shutil.copy2(b/name,d/name)
for folder in ['source-captures','repeat-source-captures','original-sources','reconstruction']:
 shutil.copytree(b/folder,d/folder,dirs_exist_ok=True)
for folder in ['gui-captures','repeat-gui-captures']:
 dest=d/folder;dest.mkdir(exist_ok=True);(dest/'display').mkdir(exist_ok=True)
 for name in ['rotation.tsv','canvas.png']:shutil.copy2(b/folder/name,dest/name)
 for name in ['results.json','server.log','authenticated-client.log','unauthenticated-client.log','command.log']:shutil.copy2(b/folder/'display'/name,dest/'display'/name)
source_files=['app/display/gimpdisplayshell-tool-events.c','app/display/gimpdisplayshell-rotate.c','app/display/gimpdisplayshell-transform.c','app/display/gimpstatusbar.c']
provenance=[]
for name in source_files:
 original=subprocess.check_output(['git','show','HEAD:'+name],cwd=b/'gimp');provenance.append({'path':name,'pinned_sha256':hashlib.sha256(original).hexdigest(),'capture_sha256':sha(b/'gimp'/name),'unchanged':original==(b/'gimp'/name).read_bytes()})
(d/'gui-source-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
def count_rows(folder):
 return len(list(csv.DictReader((b/folder/'rotation.tsv').read_text().splitlines(),delimiter='\t')))
gui_runs={}
for folder in ['gui-captures','repeat-gui-captures']:
 result=json.loads((b/folder/'display/results.json').read_text())
 markers=[int(n) for n in re.findall(r'^ROTATION_CAPTURE_COMPLETE rows=(\d+)$',(b/folder/'display/command.log').read_text(),re.M)]
 gui_runs[folder]={'rows':count_rows(folder),'completion_markers':markers,'completion_marker_matches_rows':markers==[count_rows(folder)],'command_exit':result['command_exit']}
report={'task' :'02.012','pin':'afa43fae3e920210146abed514f136fd49f671b5','capture_date':'2026-10-15','gui_rows':count_rows('gui-captures'),'source_rows':count_rows('source-captures'),'gui_runs':gui_runs,'gui_fresh_profile_exact_replay':(b/'gui-captures/rotation.tsv').read_bytes()==(b/'repeat-gui-captures/rotation.tsv').read_bytes(),'source_O0_O2_exact_replay':(b/'source-captures/rotation.tsv').read_bytes()==(b/'repeat-source-captures/rotation.tsv').read_bytes(),'gui_process_exit_primary':json.loads((b/'gui-captures/display/results.json').read_text())['command_exit'],'gui_process_exit_repeat':json.loads((b/'repeat-gui-captures/display/results.json').read_text())['command_exit'],'gui_capture_complete_before_shutdown_crash':all(r['completion_marker_matches_rows'] and r['command_exit']==-11 for r in gui_runs.values()),'clean_shutdown':all(r['command_exit']==0 for r in gui_runs.values()),'synthetic_inputs':True,'physical_input_claim':False,'gimp3_parity_claim':False,'binary_sha256':sha(b/'gimp/app/tests/.libs/test-ui'),'source_binary_sha256':sha(b/'source-capture'),'original_binary_unchanged':sha(b.parent/'legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8')=='85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1','scope':'Original GUI event handler and statusbar callback execution with synthetic inputs; separate unchanged-source+Cairo capture. Original teardown segfault retained.'}
(d/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
files=[{'path':str(p.relative_to(d)),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(d.rglob('*')) if p.is_file() and p.name!='manifest.json']
(d/'manifest.json').write_text(json.dumps({'schema_version':1,'files':files},indent=2)+'\n')
print(json.dumps(report,indent=2))

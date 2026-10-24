#!/bin/bash
set -euo pipefail
A=/workspace/shared/gimp-clean-aggregate
exec 9>"$A/build.lock"; flock 9
source "$A/env.sh"
E="$A/exact-2ef2dd32eeb7201e8e0318f169c6005a21c10410"
export E
export LD_LIBRARY_PATH=$(find "$A/build" -maxdepth 1 -type d -name 'libgimp*' -printf '%p:')${LD_LIBRARY_PATH:-}
export GIMP_TESTING_ABS_TOP_SRCDIR="$A/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$A/build" GIMP_TESTING_PLUGINDIRS="$A/build/plug-ins/common" UI_TEST=yes
python3 "$A/source/tools/run-painter-xvfb.py" python3 - <<'PY'
import subprocess,os,json,pathlib,time,hashlib
E=pathlib.Path(os.environ['E']);s=pathlib.Path(os.environ['GIMP_TESTING_ABS_TOP_SRCDIR']);b=os.environ['GIMP_TESTING_ABS_TOP_BUILDDIR'];before=json.load(open(E/'prebuild-tracked-source-sha256.json'))
def seal():
 changed=[n for n,h in before.items() if hashlib.sha256(os.fsencode(os.readlink(s/n))if(s/n).is_symlink()else(s/n).read_bytes()).hexdigest()!=h];assert not changed,changed
seal(); results=[]
try:
 cmd=[b+'/app/tests/gimp-filter-layer']
 for n in ['filter_flip_legacy_reset_contract','filter_gaussian_ancestry_contract','filter_gaussian_reentrant_ancestry_contract','filter_gaussian_restore_latest_lower_contract','filter_gaussian_translate_noop','filter_gaussian_translate_changed','filter_gaussian_scale_same','filter_gaussian_scale_changed','filter_gaussian_resize_noop','filter_gaussian_resize_changed','filter_gaussian_same_extent_matrix_contract','filter_gaussian_own_center_rotate_contract','filter_gaussian_running_duplicate_contract']:cmd+=['-p','/gimp-filter-layer/'+n]
 for name,command,env in [('filter-thirteen',cmd,dict(os.environ)),('graph-ui',['meson','test','-C',b,'--no-rebuild','--num-processes','1','--print-errorlogs','--logbase','exact-committed-graph-ui','painter-graph-invalidation'],dict(os.environ,GIMP_GRAPH_UI='1',GIMP_GRAPH_SCREENSHOTS=str(E/'graph-screenshots')))]:
  (E/'graph-screenshots').mkdir(exist_ok=True);t=time.time()
  try:
   p=subprocess.run(command,env=env,capture_output=True,text=True,timeout=180);r={'name':name,'command':command,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'duration':time.time()-t}
  except subprocess.TimeoutExpired as e:r={'name':name,'command':command,'timeout':True,'duration':time.time()-t}
  results.append(r);(E/'extra-ui-results.json').write_text(json.dumps(results,indent=2)+'\n')
finally:
 seal();(E/'extra-ui-postrun-seal.json').write_text(json.dumps({'checked_files':len(before),'changed':[]})+'\n')
assert all(r.get('returncode')==0 for r in results),results
PY

from pathlib import Path
import subprocess,json,hashlib,re
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent
binary=Path('/workspace/scratch/5b5281e79681/gimp-native-restore-20261010/build-http/app/tests/painter-perspective-ui')
sources=['app/tools/'+n for n in ['gimpperspectiveguidetool.cpp','gimpperspectiveguidetool.h','gimp-tools.c','gimptool.c','gimpdrawtool.c','tool_manager.c']]
sources+=['app/core/'+n for n in ['gimpperspectiveguide.cpp','gimpperspectiveguide.h','gimpimage-perspective-guide.c','gimpperspectiveguideundo.cpp','gimptoolinfo.c','gimptooloptions.c']]
sources+=['app/painter/'+n for n in ['binding-store.cpp','binding-store.hpp','object-ref.hpp','boundary.hpp']]
sources+=['app/tests/test-painter-perspective-ui.c','app/tests/meson.build','app/display/gimpcanvasperspectiveguide.cpp','etc/toolrc']
seals={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}
binary_hash=hashlib.sha256(binary.read_bytes()).hexdigest()
results=[]
for case in ['interrupted-add','interrupted-remove','proximity-status','interrupted-add-dispose','interrupted-remove-dispose','registered-type-options','guide-property-ownership','property-finalizer-disposes-tool','property-finalizer-replaces-model','standalone-lifetime','interrupted-motion','recursive-cancel','switch-finalizer-disposes-tool','notification-last-owner','add-move-remove-undo','close-pending-resize','cancel-hit-transforms','overlay-extents-replace','overlay-pixels-image-switch','registration-external-replace','old-toolrc-preserves-groups']:
 r=subprocess.run([str(binary),'-p','/perspective-ui/'+case],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
 (out/('native-'+case+'.log')).write_text(r.stdout)
 results.append({'case':case,'exit_code':r.returncode,'stdout':r.stdout})
 print(case,r.returncode,flush=True)
changed=[p for p,h in seals.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]
status='PASS' if len(results)==21 and all(r['exit_code']==0 and len(re.findall(r'^ok \d+ ',r['stdout'],re.M))==1 for r in results) and not changed else 'FAIL'
(out/'native.json').write_text(json.dumps({'status':status,'results':results,'binary_sha256':binary_hash,'source_sha256':seals,'changed_during_run':changed,'scope':'Original08.010 tool/options registration, actual native GTK notification/cancellation/lifetime regressions and existing ruler UI cases. No cross-platform or full-renderer acceptance.'},indent=2)+'\n')
assert status=='PASS'

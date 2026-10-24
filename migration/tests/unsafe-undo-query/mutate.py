from pathlib import Path
import json,subprocess,os,hashlib
W=Path('/workspace/shared/unsafe-undo-query');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
# Immutable old generic gate, retrieved from the frozen exact503 archive.
p=W/'old-image-undo.o';p.write_bytes(subprocess.check_output(['ar','p','/workspace/shared/device-name-identity/exact503-libappcore.a','gimpimage-undo.c.o']))
results=[]
for name,test,old,new,case in [('old-generic-gate','undo-pop-guard','gimpimage-undo.c.o',str(p),'/pop-guard/cancelable-pending'),('missing-core-veto','paired-stroke-undo','paint-core.cpp.o',str(B/'app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a.p/paint-core.cpp.o'),'/paired/idle-open-history-rejection')]:
 l=json.loads((W/(test+'-link.json')).read_text());out=W/('mutant-'+name);l[l.index('-o')+1]=str(out);l=[new if a==str(W/old)else a for a in l];subprocess.run(l,cwd=B,check=True)
 t=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text())if t['name']==test);env=os.environ.copy();ld=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['GIMP_TESTING_ABS_TOP_SRCDIR']=str(S);env['GIMP_TESTING_ABS_TOP_BUILDDIR']=str(B);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+ld
 p=subprocess.run(['python3',str(S/'tools/run-painter-xvfb.py'),str(out),'-p',case],cwd=B,env=env,capture_output=True,text=True,timeout=50);(W/(name+'.log')).write_text(p.stdout+p.stderr);results.append({'name':name,'exit':p.returncode,'expected':'nonzero causal assertion','binary_sha256':sha(out),'replacement_sha256':sha(Path(new))});assert p.returncode!=0
(W/'mutations.json').write_text(json.dumps(results,indent=2)+'\n');print(results)

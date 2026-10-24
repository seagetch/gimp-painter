from pathlib import Path
import subprocess,tempfile,json,hashlib,os
R=Path(__file__).resolve().parents[3];E=R/'migration/tests/normalize-kernel';header='app/painter/filter-normalize-kernel.hpp';visibility='app/painter/gimp-painter-visibility.h';inputs=[header,visibility,'migration/tests/normalize-kernel/test-kernel.cpp'];report={'inputs':{p:hashlib.sha256((R/p).read_bytes()).hexdigest()for p in inputs},'runs':{},'mutants':[]}
with tempfile.TemporaryDirectory(prefix='normalize-kernel-')as tmp:
 D=Path(tmp)
 for p in inputs:(D/p).parent.mkdir(parents=True,exist_ok=True);(D/p).write_bytes((R/p).read_bytes())
 def run(name,flags):
  cmd=['c++','-std=c++14','-fno-rtti','-Wall','-Wextra','-Werror','-I'+str(D),*flags,str(D/inputs[-1]),'-o',str(D/name)];b=subprocess.run(cmd,capture_output=True,text=True);assert b.returncode==0,b.stderr
  p=subprocess.run([str(D/name)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));return {'command':cmd,'build_stderr':b.stderr,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'binary_sha256':hashlib.sha256((D/name).read_bytes()).hexdigest()}
 for name,flags in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined','-fno-sanitize=vptr'])]:
  r=run(name,flags);report['runs'][name]=r;assert r['returncode']==0,r;print(name,r['stdout'].strip())
 original=(D/header).read_text()
 for name,a,b in [('wrong-scale','255*(v-low)/(high-low)','254*(v-low)/(high-low)'),('hidden-ignore','hidden_&&(hidden_low_<low_||hidden_high_>high_)','false'),('alpha-overwrite','lut_[p[2]],p[3]','lut_[p[2]],255')]:
  assert original.count(a)==1;(D/header).write_text(original.replace(a,b));r=run(name,['-O2']);r['name']=name;assert r['returncode']!=0 and 'Assertion'in r['stderr'];report['mutants'].append(r);print('REJECT',name)
report['changed']=[p for p,h in report['inputs'].items()if hashlib.sha256((R/p).read_bytes()).hexdigest()!=h];assert not report['changed'];(E/'results.json').write_text(json.dumps(report,indent=2)+'\n')

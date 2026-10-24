#!/usr/bin/env python3
import pathlib,tempfile,subprocess,json,hashlib,os,time
E=pathlib.Path(__file__).resolve().parent;sha=lambda b:hashlib.sha256(b).hexdigest();inputs={n:sha((E/n).read_bytes())for n in ['test-oracle.c','old-normalize.inc']};report={'scope':'literal old Normalize bodies in defined domain only; independent integer shared-channel oracle; no old PDB/native selection merge','inputs':inputs,'runs':{},'negative_controls':[]}
with tempfile.TemporaryDirectory(prefix='normalize-oracle-')as td:
 D=pathlib.Path(td)
 for n in inputs:(D/n).write_bytes((E/n).read_bytes())
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(name,flags):
  exe=D/name;cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,str(D/'test-oracle.c'),'-lm','-o',str(exe)];p=subprocess.run(cmd,capture_output=True,text=True);assert p.returncode==0,p.stderr;start=time.monotonic();q=subprocess.run([str(exe)],capture_output=True,text=True,env=env,timeout=120);return dict(command=cmd,build_stdout=p.stdout,build_stderr=p.stderr,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,seconds=time.monotonic()-start,binary_sha256=sha(exe.read_bytes()))
 for name,flags in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer'])]:
  r=run(name,flags);report['runs'][name]=r;assert r['returncode']==0,r;print(name,r['stdout'].strip())
 old=(E/'old-normalize.inc').read_text()
 controls=[('ignore-partial-alpha','!param->has_alpha || src[param->alpha]','!param->has_alpha || src[param->alpha] == 255'),('overwrite-alpha','dest[param->alpha] = src[param->alpha];','dest[param->alpha] = 255;'),('wrong-scale','255 * (x - param.min) / range','254 * (x - param.min) / range'),('flat-zero','param.lut[(gint)param.min] = param.min;','param.lut[(gint)param.min] = 0;')]
 for name,before,after in controls:
  assert old.count(before)==1;(D/'old-normalize.inc').write_text(old.replace(before,after));r=run(name,['-O2']);r['name']=name;r['mutant_sha256']=sha((D/'old-normalize.inc').read_bytes());assert r['returncode']!=0 and 'Assertion'in r['stderr'],r;report['negative_controls'].append(r);print('REJECT',name)
report['inputs_changed']=[n for n,h in inputs.items()if sha((E/n).read_bytes())!=h];assert not report['inputs_changed'];(E/'results.json').write_text(json.dumps(report,indent=2)+'\n')

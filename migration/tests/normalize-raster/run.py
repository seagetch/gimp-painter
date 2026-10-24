#!/usr/bin/env python3
import pathlib,subprocess,tempfile,json,hashlib,os,shlex,tarfile,io
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
inputs=['app/painter/filter-normalize-raster.hpp','app/painter/filter-normalize-raster.cpp','app/painter/filter-normalize-kernel.hpp','app/painter/filter-raster.hpp','app/painter/filter-raster.cpp','app/painter/filter-progress.hpp','app/painter/gimp-painter-visibility.h','migration/tests/normalize-raster/test-raster.cpp']
report={'inputs':{p:sha((R/p).read_bytes())for p in inputs},'runs':{},'mutants':[],'instrumented_units':['app/painter/filter-normalize-raster.cpp','app/painter/filter-raster.cpp','migration/tests/normalize-raster/test-raster.cpp']}
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
with tempfile.TemporaryDirectory(prefix='normalize-raster-')as tmp:
 D=pathlib.Path(tmp)
 for p in inputs:(D/p).parent.mkdir(parents=True,exist_ok=True);(D/p).write_bytes((R/p).read_bytes())
 def run(name,extra):
  cmd=['c++','-std=c++14','-fno-rtti','-Wall','-Wextra','-Werror','-I'+str(D),*extra,*[str(D/p)for p in report['instrumented_units']],*flags,'-pthread','-o',str(D/name)]
  b=subprocess.run(cmd,capture_output=True,text=True);assert b.returncode==0,b.stderr
  q=subprocess.run([str(D/name)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  return {'command':cmd,'build_stderr':b.stderr,'returncode':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'binary_sha256':sha((D/name).read_bytes())}
 for name,extra in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined','-fno-sanitize=vptr'])]:
  q=run(name,extra);report['runs'][name]=q;assert q['returncode']==0,q;print(name,q['stdout'].strip())
 target=D/inputs[1];old=target.read_text()
 for name,a,b in [('omit-flush','output.flush();','/* omitted flush */'),('incomplete-first-pass','FilterNormalizeStats stats;','FilterNormalizeStats stats; stats.add({{0,255,0,255}});'),('cancel-after-flush','if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;\n  if(progress)progress->set_value(1.0);','if(progress)progress->set_value(1.0);')]:
  assert old.count(a)==1;target.write_text(old.replace(a,b));q=run(name,['-O2']);q['name']=name;q['mutant_sha256']=sha(target.read_bytes());assert q['returncode']!=0 and 'Assertion'in q['stderr'],q;report['mutants'].append(q);print('REJECT',name)
report['changed']=[p for p,h in report['inputs'].items()if sha((R/p).read_bytes())!=h];assert not report['changed'];(E/'results.json').write_text(json.dumps(report,indent=2)+'\n')
with tarfile.open(E/'sources.tar.gz','w:gz')as t:
 for p in inputs:t.add(R/p,arcname=p)

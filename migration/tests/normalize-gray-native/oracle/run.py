#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,json,os,hashlib,resource
E=Path(__file__).resolve().parent;R=E.parents[3];sha=lambda b:hashlib.sha256(b).hexdigest();rows=[]
with tempfile.TemporaryDirectory(prefix='normalize-gray-') as td:
 D=Path(td)
 for n in ['old-host.c','test-oracle.c','old-normalize.inc','test.cpp']:(D/n).write_bytes((E/n).read_bytes())
 header=(R/'app/painter/filter-normalize-kernel.hpp').read_bytes();(D/'filter-normalize-kernel.hpp').write_bytes(header)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 def run(name,flags):
  commands=[['cc','-std=c11',*flags,'-c',str(D/'old-host.c'),'-o',str(D/'old.o')],['c++','-std=c++14','-fno-rtti',*flags,'-I'+str(R/'app/painter'),str(D/'test.cpp'),str(D/'old.o'),'-o',str(D/'test')]]
  for c in commands:subprocess.run(c,check=True,capture_output=True,text=True)
  q=subprocess.run([str(D/'test')],capture_output=True,text=True,env=env,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  row=dict(name=name,commands=commands,code=q.returncode,stdout=q.stdout,stderr=q.stderr,binary_sha256=sha((D/'test').read_bytes()),header_sha256=sha((D/'filter-normalize-kernel.hpp').read_bytes()));rows.append(row);return row
 for name,flags in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer'])]:
  q=run(name,flags);assert q['code']==0 and '65792' in q['stdout'];print(name,q['stdout'].strip())
 for name,before,after in [('scale','255*(v-low)/(high-low)','254*(v-low)/(high-low)'),('flat','low==high?v:','low==high?0:'),('alpha','p[3]}};','std::uint8_t(255)}};')]:
  s=header.decode();assert s.count(before)==1;(D/'filter-normalize-kernel.hpp').write_text(s.replace(before,after));q=run(name,['-O2']);assert q['code']!=0 and 'Assertion' in q['stderr'];print('REJECT',name)
(E/'results.json').write_text(json.dumps(dict(runs=rows,inputs={n:sha((E/n).read_bytes())for n in ['old-host.c','test-oracle.c','old-normalize.inc','test.cpp']},kernel_sha256=sha(header)),indent=2)+'\n')

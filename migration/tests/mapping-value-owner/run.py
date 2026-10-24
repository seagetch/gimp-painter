#!/usr/bin/env python3
"""Replay the pure C++14 value-owner matrix and causal implementation controls."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
paths=['app/paint/painter-mypaint/mapping.hpp','app/painter/gimp-painter-visibility.h','migration/tests/mapping-value-owner/test.cpp'];hashes={n:hashlib.sha256((root/n).read_bytes()).hexdigest()for n in paths}
results=[]
def run(name,cmd,env=None):
 t=time.monotonic();r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=60);results.append(dict(name=name,command=cmd,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr,duration=time.monotonic()-t));return r
base=['c++','-std=c++14','-Wall','-Wextra','-Werror','-O1','-g'];test=root/paths[2]
try:
 for kind,flags in [('normal',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  exe=out/kind;assert run(kind+'-compile',base+flags+['-I',str(root),str(test),'-o',str(exe)]).returncode==0
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1');r=run(kind,[str(exe)],env);assert r.returncode==0 and 'scenarios=80 clear-counter=1' in r.stdout
 mutations={
  'copy-loses-curves':('  std::size_t storage_bytes', '  Mapping (const Mapping& m) : points_(m.points_.size()), base_value(m.base_value) {}\n  Mapping& operator=(const Mapping&) = default;\n  std::size_t storage_bytes'),
  'assignment-retains-old':('  std::size_t storage_bytes','  Mapping (const Mapping&) = default;\n  Mapping& operator=(const Mapping&) { return *this; }\n  std::size_t storage_bytes'),
  'clear-stale-counter':('    if (!n && p.n) --inputs_used_;','    if (!n && p.n) {}'),
 }
 for name,(old,new)in mutations.items():
  with tempfile.TemporaryDirectory(prefix='mapping-mutant-',dir=out)as d:
   d=pathlib.Path(d)
   for n in paths[:2]:q=d/n;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes((root/n).read_bytes())
   h=d/paths[0];s=h.read_text();assert s.count(old)==1;h.write_text(s.replace(old,new));exe=d/'test'
   assert run(name+'-compile',base+['-I',str(d),str(test),'-o',str(exe)]).returncode==0
   r=run(name,[str(exe)]);assert r.returncode!=0 and 'Assertion' in r.stderr,(name,r)
finally:
 changed=[n for n,h in hashes.items()if hashlib.sha256((root/n).read_bytes()).hexdigest()!=h]
 (out/'results.json').write_text(json.dumps({'scope':'Standalone real Mapping value owner; not app/runtime/old invalid allocator execution. LSan disabled.','source_sha256':hashes,'changed_after':changed,'results':results},indent=2)+'\n')
 assert not changed
print('PASS: 80 ownership scenarios plus counter reset, normal and ASan/UBSan; 3 causal mutants rejected')

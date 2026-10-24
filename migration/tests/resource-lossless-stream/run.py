#!/usr/bin/env python3
import argparse,hashlib,json,os,pathlib,shlex,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[3];e=pathlib.Path(__file__).resolve().parent;o=pathlib.Path(a.output).resolve();o.mkdir(parents=True,exist_ok=True)
d=pathlib.Path('/workspace/shared/gimp-deps-locked/root/usr');lib=d/'lib/x86_64-linux-gnu'
inputs=['app/paint/painter-mypaint/resource.cpp','app/paint/painter-mypaint/resource.hpp','app/paint/painter-mypaint/mapping.hpp','app/painter/gimp-painter-error.cpp','app/painter/gimp-painter-error.h','app/painter/gimp-painter-visibility.h','app/painter/object-ref.hpp','app/painter/boundary.hpp','app/painter/resources.hpp','migration/tests/resource-lossless-stream/test.cpp','migration/tests/resource-lossless-stream/run.py']
for n in ['mypaintbrush-settings-data.h','mypaintbrush-enum-settings.h']:
 target=e/'generated'/n;target.parent.mkdir(exist_ok=True)
 if not target.exists():target.write_bytes((pathlib.Path('/workspace/shared/gimp-clean-aggregate/build/app/paint/painter-brush-settings')/n).read_bytes())
 inputs.append(str(target.relative_to(r)))
hashes={n:hashlib.sha256((r/n).read_bytes()).hexdigest()for n in inputs}
base=['c++','-std=c++14','-O1','-g','-Wall','-Wextra','-I'+str(r/'app'),'-I'+str(r/'app/paint/painter-mypaint'),'-I'+str(e/'generated')]
base+=['-I'+str(d/x)for x in ['include/glib-2.0','lib/x86_64-linux-gnu/glib-2.0/include','include/json-glib-1.0']]
libs=[str(lib/('lib'+n+'.so'))for n in ['json-glib-1.0','gio-2.0','gobject-2.0','glib-2.0']]
libs+=['-Wl,-rpath,'+str(lib),'-Wl,--dynamic-linker='+str(lib/'ld-linux-x86-64.so.2'),'-pthread']
dependencies={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [lib/('lib'+n+'.so') for n in ['json-glib-1.0','gio-2.0','gobject-2.0','glib-2.0']]+[lib/'ld-linux-x86-64.so.2',lib/'libasan.so.8',lib/'libubsan.so.1']}
compiler=subprocess.check_output(['c++','--version'],text=True)
base_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip()
records=[]
def run(name,cmd,timeout=90):
 t=time.monotonic();env=dict(os.environ,LD_LIBRARY_PATH=str(lib),ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 z=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=timeout);records.append(dict(name=name,command=cmd,returncode=z.returncode,stdout=z.stdout,stderr=z.stderr,seconds=time.monotonic()-t));return z
try:
 for kind,flags in [('normal',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  exe=o/kind;cmd=base+flags+[str(r/inputs[0]),str(r/'app/painter/gimp-painter-error.cpp'),str(e/'test.cpp'),'-o',str(exe)]+libs
  z=run(kind+'-compile',cmd);assert z.returncode==0,z.stderr
  z=run(kind,[str(exe)]);print(kind,z.returncode,z.stdout,z.stderr,flush=True);assert z.returncode==0
 mutations={
 'drop-unknown-object':('      const auto *key = static_cast<const char *> (p->data);','      const auto *key = static_cast<const char *> (p->data); if (std::string(key) == "vendor") continue;','/resource-proof/lossless'),
 'unknown-int64-to-double':('  return Node (json_node_copy (n));','  if (JSON_NODE_HOLDS_VALUE(n) && json_node_get_value_type(n)==G_TYPE_INT64) { Node out(json_node_new(JSON_NODE_VALUE)); json_node_set_double(out.get(),json_node_get_double(n)); return out; } return Node (json_node_copy (n));','/resource-proof/lossless'),
 'missing-switch-true':('  if (!n) return false;','  if (!n) return true;','/resource-proof/defaults-texts'),
 'save-hides-error':('    return bool (g_output_stream_write_all (output, text.data (), text.size (), nullptr, cancel, error));','    g_output_stream_write_all (output, text.data (), text.size (), nullptr, cancel, error); return true;','/resource-proof/output')}
 for name,(old,new,group)in mutations.items():
  source=(r/inputs[0]).read_text();assert source.count(old)==1
  mutant=o/(name+'.cpp');mutant.write_text(source.replace(old,new));exe=o/name
  z=run(name+'-compile',base+[str(mutant),str(r/'app/painter/gimp-painter-error.cpp'),str(e/'test.cpp'),'-o',str(exe)]+libs);assert z.returncode==0,z.stderr
  z=run(name,[str(exe),'-p',group]);assert z.returncode!=0 and ('assertion failed' in z.stderr or 'should be' in z.stderr),z.stderr
finally:
 changed=[n for n,h in hashes.items()if hashlib.sha256((r/n).read_bytes()).hexdigest()!=h]
 (o/'results.json').write_text(json.dumps({'base_commit':base_commit,'compiler':compiler,'dependency_sha256':dependencies,'source_sha256':hashes,'changed_after':changed,'scope':'Standalone Resource/error/test three compiled units normal and ASan/UBSan; dependency libraries uninstrumented; LSan disabled; no held providers or whole-app claim','results':records},indent=2)+'\n')
 assert not changed
print('PASS four structural/default/text/output/malformed groups normal+sanitizer; four compiled causal mutants')

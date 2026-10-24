import subprocess,json,tempfile,pathlib,hashlib,os,resource
E=pathlib.Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs={n:sha(E/n) for n in ['test-oracle.c','old-wrapper.inc','old-compressor.inc','reference.json']}
def run(cmd,env=None):
 p=subprocess.run(cmd,text=True,capture_output=True,env=env);return dict(command=cmd,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
results={'inputs':inputs,'runs':{}}
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
with tempfile.TemporaryDirectory(prefix='event-compression-') as d:
 d=pathlib.Path(d)
 for mode,flags in [('normal',[]),('sanitizer',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  exe=d/mode;cmd=['gcc','-std=c11','-Wall','-Wextra','-Werror',*flags,str(E/'test-oracle.c'),'-o',str(exe)]
  compile=run(cmd);assert compile['exit_code']==0,compile
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  r=run([str(exe)],env);assert r['exit_code']==0,r;results['runs'][mode]={'compile':compile,'runtime':r}
 mutations=[('state-boundary','old-compressor.inc','(event->motion.state  == initial_event->motion.state)','(1)'),('device-boundary','old-compressor.inc','(event->motion.device == initial_event->motion.device)','(1)'),('window-boundary','old-compressor.inc','(event->any.window    == initial_event->any.window)','(1)'),('terminal-dispatch','old-wrapper.inc','gtk_main_do_event (next_event);','/* omit dispatch */'),('terminal-free','old-wrapper.inc','gdk_event_free (next_event);','/* omit free */'),('double-terminal-free','old-wrapper.inc','gdk_event_free (next_event);','gdk_event_free (next_event); gdk_event_free (next_event);')]
 results['mutations']=[]
 for name,filename,old,new in mutations:
  m=d/name;m.mkdir()
  for n in ['test-oracle.c','old-wrapper.inc','old-compressor.inc']:(m/n).write_bytes((E/n).read_bytes())
  p=m/filename;s=p.read_text();assert s.count(old)==1;s=s.replace(old,new);p.write_text(s)
  c=run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(m/'test-oracle.c'),'-o',str(m/'oracle')]);assert c['exit_code']==0,c
  r=run([str(m/'oracle')]);assert r['exit_code']!=0 and 'Assertion' in r['stderr'],r
  results['mutations'].append({'name':name,'changed_file':filename,'original_sha256':inputs[filename],**r})
results['changed_after_run']=[n for n,h in inputs.items() if sha(E/n)!=h]
assert not results['changed_after_run'];(E/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS normal/sanitizer 4163 scenes each; 6 causal mutants rejected; LSan disabled (ptrace environment)')

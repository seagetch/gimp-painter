import pathlib,json,os,subprocess,time,sys,hashlib,re
O=pathlib.Path('/workspace/shared/painter-time-sequences');A=pathlib.Path('/workspace/shared/gimp-clean-aggregate');env=os.environ.copy();r=next(r for r in json.loads((A/'build/meson-info/intro-tests.json').read_text())if r['name']=='painter-mypaint-tool');ld=env.get('LD_LIBRARY_PATH','');env.update(r['env']);env['LD_LIBRARY_PATH']+=':'+ld
for key,sub in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
 p=O/'profile'/sub;p.mkdir(parents=True,exist_ok=True);env[key]=str(p)
env['UI_TEST']='yes';env.pop('GIMP_TEST_REQUIRE_PORTRAIT',None);
if os.environ.get('CONTROL_REQUIRE_PORTRAIT')=='1':env['GIMP_TEST_REQUIRE_PORTRAIT']='1'
(O/'screenshots').mkdir(exist_ok=True);env['GIMP_TEST_ONCANVAS_SCREENSHOTS']=str(O/'screenshots')
mode=sys.argv[1] if len(sys.argv)>1 else 'red'; env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1';cmd=['python3',str(O/'run-landscape-xvfb.py'),str(O/os.environ.get('TIMESTAMP_BINARY','timestamp-sequences')),*sys.argv[2:]];binary=O/os.environ.get('TIMESTAMP_BINARY','timestamp-sequences');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();loader=subprocess.check_output(['ldd',str(binary)],env=env,text=True);deps={str(binary):sha(binary)}
for line in loader.splitlines():
 for value in line.split():
  q=pathlib.Path(value)
  if value.startswith('/') and q.is_file():deps[str(q)]=sha(q)
start=time.monotonic();p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=180);(O/('expected-red.log' if mode=='red' else mode+'.log')).write_text(p.stdout+p.stderr);(O/('expected-red.json' if mode=='red' else mode+'.json')).write_text(json.dumps({'command':cmd,'returncode':p.returncode,'seconds':time.monotonic()-start,'stdout':p.stdout,'stderr':p.stderr,'scope':'direct registered-tool callbacks forwarded to real native session; exact released host plus stamped private fixture; no GDK queued input, physical hardware, old reverse/equal parity or Windows claim','binary_and_runtime_inputs':deps,'runtime_inputs_unchanged':all(sha(pathlib.Path(p))==h for p,h in deps.items()),'ldd':loader},indent=2)+'\n');print(p.returncode);print((p.stdout+p.stderr)[-3500:])

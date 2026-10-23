#!/usr/bin/env python3
"""Run an explicit command against bounded, authenticated loopback-only Xvfb."""
import argparse, datetime, json, os, pathlib, secrets, shutil, signal, socket, subprocess, sys, tempfile, time
p=argparse.ArgumentParser(); p.add_argument('--out', type=pathlib.Path, required=True); p.add_argument('--timeout',type=int,default=1800); p.add_argument('--screen',choices=['1600x1000x24','1080x1920x24'],default='1600x1000x24'); p.add_argument('command',nargs=argparse.REMAINDER); a=p.parse_args()
if a.command and a.command[0]=='--': a.command=a.command[1:]
if not a.command: p.error('command required')
if not 0<a.timeout<=3600: p.error('timeout must be 1..3600')
a.out=a.out.resolve(); a.out.mkdir(parents=True,exist_ok=True)
assert not os.environ.get('LD_PRELOAD') and not os.environ.get('LD_AUDIT')
os.umask(0o077)
private=pathlib.Path(tempfile.mkdtemp(prefix='display-private-',dir=a.out)); auth=private/'Xauthority'; empty=private/'empty'; auth.touch(mode=0o600); empty.touch(mode=0o600)
r={'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'command':a.command,'timeout':a.timeout}; child=None; listener=None; result_code=1
try:
 listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM); listener.bind(('127.0.0.1',0)); address,port=listener.getsockname(); number=port-6000; assert number>=0; listener.listen(16)
 display=f'127.0.0.1:{number}'; r['listener']=[address,port]
 subprocess.run(['/usr/bin/xauth','-f',str(auth),'-q'],input=f'add {display} MIT-MAGIC-COOKIE-1 {secrets.token_hex(16)}\n',text=True,check=True,timeout=10)
 xvfb=pathlib.Path(os.environ['GIMP_DEPS_ROOT'])/'usr/bin/Xvfb'; r['xvfb']=str(xvfb)
 argv=[str(xvfb),f':{number}','-nolisten','tcp','-nolisten','unix','-nolisten','local','-auth',str(auth),'-noreset','-screen','0',a.screen]
 child=os.fork()
 if child==0:
  try:
   if listener.fileno()!=3: os.dup2(listener.fileno(),3,inheritable=True)
   else: os.set_inheritable(3,True)
   fd=os.open(str(a.out/'server.log'),os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600); os.dup2(fd,1); os.dup2(fd,2)
   if fd>3: os.close(fd)
   env=dict(os.environ,LISTEN_PID=str(os.getpid()),LISTEN_FDS='1'); env.pop('LISTEN_FDNAMES',None); os.execve(str(xvfb),argv,env)
  except BaseException as e: print(str(e),file=sys.stderr,flush=True); os._exit(127)
 listener.close(); listener=None
 env=dict(os.environ,DISPLAY=display,XAUTHORITY=str(auth))
 for attempt in range(40):
  q=subprocess.run(['/usr/bin/xdpyinfo'],env=env,text=True,capture_output=True,timeout=5)
  if q.returncode==0: break
  time.sleep(.25)
 (a.out/'authenticated-client.log').write_text(q.stdout+q.stderr); r['authenticated_client_exit']=q.returncode
 if q.returncode: raise RuntimeError('Authenticated display unavailable')
 q=subprocess.run(['/usr/bin/ss','-ltnp'],text=True,capture_output=True,check=True,timeout=10); lines=[l for l in q.stdout.splitlines() if f'pid={child},' in l]; r['listeners']=lines
 if len(lines)!=1 or f'127.0.0.1:{port}' not in lines[0]: raise RuntimeError('Expected exactly one loopback listener')
 q=subprocess.run(['/usr/bin/xdpyinfo'],env=dict(env,XAUTHORITY=str(empty)),text=True,capture_output=True,timeout=5); (a.out/'unauthenticated-client.log').write_text(q.stdout+q.stderr); r['unauthenticated_client_exit']=q.returncode
 if q.returncode==0: raise RuntimeError('Unauthenticated access succeeded')
 r['transport_validation']='PASS'
 for key,folder in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
  directory=private/folder; directory.mkdir(mode=0o700); env[key]=str(directory)
 for folder in ['cache/gimp/3.0','tmp/gimp/3.0']: (private/folder).mkdir(parents=True,exist_ok=True)
 # Registered Meson fixtures retain their own GSettings and data decisions.
 env.pop('GSETTINGS_BACKEND',None); env.pop('GIMP3_DATADIR',None)
 (a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n'); print('Authenticated loopback display validated; running command.',flush=True)
 with (a.out/'command.log').open('w') as log:
  q=subprocess.run(a.command,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=a.timeout)
 result_code=q.returncode; r['command_exit']=result_code
except BaseException as e:
 r['error']=f'{type(e).__name__}: {e}'; print(r['error'],file=sys.stderr)
finally:
 if listener: listener.close()
 if child:
  try:
   os.kill(child,signal.SIGTERM)
   for _ in range(50):
    ended,status=os.waitpid(child,os.WNOHANG)
    if ended: break
    time.sleep(.1)
   else: os.kill(child,signal.SIGKILL); os.waitpid(child,0)
  except ProcessLookupError: pass
 shutil.rmtree(private); r['cleanup']='server stopped and temporary authorization/profile removed'; r['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat(); (a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
sys.exit(result_code)

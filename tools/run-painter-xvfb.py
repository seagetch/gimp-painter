#!/usr/bin/env python3
"""Run a bounded command on authenticated loopback-only 1080x1920 Xvfb.

Requires Xvfb with systemd listener activation, xauth, libX11 and a writable
TMPDIR. No local-domain X sockets or unauthenticated listeners are created.
"""
import os, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path
if len(sys.argv) < 2:
    raise SystemExit('usage: run-painter-xvfb.py COMMAND [ARGS...]')
with tempfile.TemporaryDirectory(prefix='xvfb-auth-', dir=os.environ['TMPDIR']) as temporary:
    auth=Path(temporary)/'authority'; auth.touch(mode=0o600)
    empty=Path(temporary)/'empty'; empty.touch(mode=0o600)
    listener=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    for number in range(91, 191):
        if Path(f'/tmp/.X{number}-lock').exists(): continue
        try: listener.bind(('127.0.0.1',6000+number)); break
        except OSError: continue
    else: raise RuntimeError('No free loopback display port')
    listener.listen(32)
    display=f'127.0.0.1:{number}'
    subprocess.run(['xauth','-f',str(auth),'add',display,'MIT-MAGIC-COOKIE-1',os.urandom(16).hex()],check=True)
    env=os.environ.copy(); env.update(DISPLAY=display,XAUTHORITY=str(auth))
    launcher=f'import os; os.dup2({listener.fileno()},3,inheritable=True); os.environ.update(LISTEN_PID=str(os.getpid()),LISTEN_FDS="1"); os.execvp("Xvfb",["Xvfb"]+__import__("sys").argv[1:])'
    args=[sys.executable,'-c',launcher,f':{number}','-screen','0','1080x1920x24','-auth',str(auth),'-nolisten','tcp','-nolisten','unix','-nolisten','local','-noreset']
    server=subprocess.Popen(args,env=env,pass_fds=(listener.fileno(),),stdout=subprocess.DEVNULL)
    probe='import ctypes; x=ctypes.CDLL("libX11.so.6"); x.XOpenDisplay.restype=ctypes.c_void_p; p=x.XOpenDisplay(None); x.XCloseDisplay.argtypes=[ctypes.c_void_p]; x.XCloseDisplay(p) if p else None; raise SystemExit(0 if p else 1)'
    try:
        for _ in range(30):
            if server.poll() is not None: raise RuntimeError('Xvfb exited during startup')
            try: result=subprocess.run([sys.executable,'-c',probe],env=env,timeout=2,stderr=subprocess.DEVNULL)
            except subprocess.TimeoutExpired: time.sleep(.2); continue
            if result.returncode==0: break
            time.sleep(.2)
        else: raise RuntimeError('Authenticated X client failed')
        bad=env.copy();bad['XAUTHORITY']=str(empty)
        rejected=subprocess.run([sys.executable,'-c',probe],env=bad,timeout=5,stderr=subprocess.DEVNULL)
        if rejected.returncode==0: raise RuntimeError('Unauthenticated X client accepted')
        if listener.getsockname()!=('127.0.0.1',6000+number): raise RuntimeError('Unexpected listener')
        print(f'Xvfb verified: {display}, 1080x1920x24, authenticated accepted, empty authority rejected',flush=True)
        command=subprocess.Popen(sys.argv[1:],env=env,start_new_session=True)
        try:
            status=command.wait(timeout=3500)
        finally:
            # Only the new command's process group; never unrelated host tasks.
            try: os.killpg(command.pid,signal.SIGTERM)
            except ProcessLookupError: pass
            try: command.wait(timeout=5)
            except subprocess.TimeoutExpired: pass
            time.sleep(.2)
            try: os.killpg(command.pid,signal.SIGKILL)
            except ProcessLookupError: pass
            command.wait()
        raise SystemExit(status)
    finally:
        server.terminate()
        try: server.wait(timeout=5)
        except subprocess.TimeoutExpired: server.kill();server.wait()
        listener.close()

import ctypes as C, json, os, subprocess, time, socket, secrets, urllib.request, hashlib
from pathlib import Path
import gi
gi.require_version('Gdk','3.0')
from gi.repository import Gdk
Gdk.init([])
base=Path('/workspace/shared/gimp-native-baseline')
out=Path('/workspace/shared/gimp-http-app-exit')
profile=out/'profile'; profile.mkdir(exist_ok=True)
(profile/'gimprc').write_text('(painter-canvas-ui yes)\n(show-welcome-dialog no)\n')
env=os.environ.copy(); env['GIMP3_DIRECTORY']=str(profile)
token=secrets.token_urlsafe(36)
with socket.socket() as sock:
 sock.bind(('127.0.0.1',0)); port=sock.getsockname()[1]
env.update(GIMP_PAINTER_HTTP_ENABLE='1', GIMP_PAINTER_HTTP_PORT=str(port), GIMP_PAINTER_HTTP_TOKEN=token)
log=open(out/'app.log','w')
p=subprocess.Popen([str(base/'prefix/bin/gimp-3.0'),'--verbose','--no-splash','--new-instance',str(base/'source/migration/fixtures/gimp3-baseline.xcf')],env=env,stdout=log,stderr=subprocess.STDOUT)
x=C.CDLL('libX11.so.6'); x.XOpenDisplay.restype=C.c_void_p; d=x.XOpenDisplay(None)
x.XDefaultRootWindow.argtypes=[C.c_void_p]; x.XDefaultRootWindow.restype=C.c_ulong
root=x.XDefaultRootWindow(d)
x.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]
x.XFetchName.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_char_p)]
x.XFree.argtypes=[C.c_void_p]
x.XGetGeometry.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
def size(w):
 r=C.c_ulong(); xx=C.c_int(); yy=C.c_int(); ww=C.c_uint(); hh=C.c_uint(); b=C.c_uint(); depth=C.c_uint()
 x.XGetGeometry(d,w,C.byref(r),C.byref(xx),C.byref(yy),C.byref(ww),C.byref(hh),C.byref(b),C.byref(depth))
 return ww.value,hh.value

def windows():
 r=C.c_ulong(); parent=C.c_ulong(); children=C.POINTER(C.c_ulong)(); n=C.c_uint()
 x.XQueryTree(d,root,C.byref(r),C.byref(parent),C.byref(children),C.byref(n))
 out=[]
 for i in range(n.value):
  name=C.c_char_p()
  if x.XFetchName(d,children[i],C.byref(name)) and name.value:
   out.append((children[i],name.value.decode(errors='replace'))); x.XFree(name)
  else: out.append((children[i],''))
 x.XFree(children)
 return out
x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
x.XFlush.argtypes=[C.c_void_p]
x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong]; x.XKeysymToKeycode.restype=C.c_uint
xt=C.CDLL('libXtst.so.6'); xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
win=None
welcome_closed=False
try:
 for _ in range(120):
  if p.poll() is not None: raise RuntimeError(f'GIMP exited {p.returncode}')
  all_windows=windows()
  welcome=[w for w,t in all_windows if 'Welcome to GIMP' in t]
  if welcome and not welcome_closed:
   x.XSetInputFocus(d,welcome[0],2,0)
   esc=x.XKeysymToKeycode(d,0xff1b)
   xt.XTestFakeKeyEvent(d,esc,1,0); xt.XTestFakeKeyEvent(d,esc,0,0); x.XFlush(d)
   welcome_closed=True
  found=[(w,t) for w,t in all_windows if size(w)[0]>=500 and size(w)[1]>=300] if not welcome else []
  if found: win=found[0][0]; break
  time.sleep(.5)
 if win is None: raise RuntimeError('Image window not found: '+repr(windows()))
 x.XMoveResizeWindow.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_int,C.c_uint,C.c_uint]
 x.XMoveResizeWindow(d,win,0,0,1080,1920)
 x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]; x.XSetInputFocus(d,win,2,0)
 x.XFlush.argtypes=[C.c_void_p]; x.XFlush(d); time.sleep(4)
 for attempt in range(12):
  candidates=[w for w,t in windows() if size(w)[0]>=500 and size(w)[1]>=300]
  if candidates: win=candidates[0]
  x.XMoveResizeWindow(d,win,0,0,1080,1920); x.XSetInputFocus(d,win,2,0); x.XFlush(d); time.sleep(1)
  if size(win)==(1080,1920): break
 if size(win)!=(1080,1920): raise RuntimeError('Main geometry wrong: '+repr(size(win)))
 verified_size=size(win)
 rootwin=Gdk.get_default_root_window(); pix=Gdk.pixbuf_get_from_window(rootwin,0,0,1080,1920)
 pix.savev(str(out/'portrait-oncanvas-http.png'),'png',[],[])
 request=urllib.request.Request(f'http://127.0.0.1:{port}/api/v1/images/',headers={'Authorization':'Bearer '+token})
 with urllib.request.urlopen(request,timeout=10) as response:
  http_status=response.status; http_body=response.read().decode()
 if http_status!=200:raise RuntimeError('HTTP route unavailable')
 xt=C.CDLL('libXtst.so.6'); xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
 x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong]; x.XKeysymToKeycode.restype=C.c_uint
 ctrl=x.XKeysymToKeycode(d,0xffe3); q=x.XKeysymToKeycode(d,ord('q'))
 for key,down in [(ctrl,1),(q,1),(q,0),(ctrl,0)]: xt.XTestFakeKeyEvent(d,key,down,0)
 x.XFlush(d)
 code=p.wait(timeout=30)
 log.flush()
 text=(out/'app.log').read_text()
 assert token not in text and token not in http_body, 'Ephemeral authentication material appeared in output'
 assert 'EXIT: app_exit_after_callback' in text, 'Accepted app exit-after hook absent'
 with socket.socket() as probe: port_closed=probe.connect_ex(('127.0.0.1',port))!=0
 assert port_closed
 names=['app/app.c','app/core/gimp.c','app/core/gimp-data-factories.c','app/httpd/httpd.cpp','app/display/gimppaintercanvasui.cpp','app/widgets/gimppainterlayertiles.cpp','app/widgets/gimppaintermybrusheditor.cpp']
 (out/'result.json').write_text(json.dumps({'exit_code':code,'resolution':[1080,1920],'painter_canvas_ui':True,'window_size':verified_size,'http_status':http_status,'http_body':http_body,'http_listener_closed_after_exit':port_closed,'accepted_exit_hook_observed':True,'app_sha256':hashlib.sha256((base/'prefix/bin/gimp-3.0').read_bytes()).hexdigest(),'sources_sha256':{n:hashlib.sha256((base/'source'/n).read_bytes()).hexdigest() for n in names}},indent=2)+'\n')
 if code: raise RuntimeError(f'GIMP returned {code}')
finally:
 if p.poll() is None: p.terminate(); p.wait(timeout=10)
 log.close()

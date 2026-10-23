import ctypes as C, json, os, subprocess, time
from pathlib import Path
import gi
gi.require_version('Gdk','3.0')
from gi.repository import Gdk
Gdk.init([])
base=Path('/workspace/shared/gimp-native-baseline')
profile=base/'editor-memory-profile'; profile.mkdir(exist_ok=True)
(profile/'gimprc').write_text('(painter-canvas-ui yes)\n(show-welcome-dialog no)\n')
env=os.environ.copy(); env['GIMP3_DIRECTORY']=str(profile)
log=open(base/'editor-memory-app.log','w')
p=subprocess.Popen([str(base/'prefix/bin/gimp-3.0'),'--no-splash','--new-instance',str(base/'source/migration/fixtures/gimp3-baseline.xcf')],env=env,stdout=log,stderr=subprocess.STDOUT)
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
 if size(win)!=(1080,1920): raise RuntimeError('Main geometry wrong: '+repr(size(win)))
 verified_size=size(win)
 rootwin=Gdk.get_default_root_window()
 commands=base/'editor-commands'; commands.mkdir(exist_ok=True)
 output=base/'editor-memory-evidence'; output.mkdir(exist_ok=True)
 def screenshot(name):
  pix=Gdk.pixbuf_get_from_window(rootwin,0,0,1080,1920)
  pix.savev(str(output/(name+'.png')),'png',[],[])
 def key(sym):
  code=x.XKeysymToKeycode(d,sym)
  if not code: raise RuntimeError('No keycode for '+str(sym))
  xt.XTestFakeKeyEvent(d,code,1,0); xt.XTestFakeKeyEvent(d,code,0,0); x.XFlush(d)
 def text(value):
  for ch in value: key(ord(ch)); time.sleep(.03)
 xt.XTestFakeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ulong]
 xt.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
 def click(xx,yy):
  xt.XTestFakeMotionEvent(d,-1,xx,yy,0)
  xt.XTestFakeButtonEvent(d,1,1,0); xt.XTestFakeButtonEvent(d,1,0,0); x.XFlush(d)
 screenshot('00-main-portrait')
 key(ord('/')); time.sleep(1)
 search=[w for w,t in windows() if t=='Search Actions']
 if not search: raise RuntimeError('Search Actions window missing')
 x.XSetInputFocus(d,search[0],2,0); x.XFlush(d)
 text('painter mypaint brush editor'); time.sleep(2); screenshot('01-action-search'); key(0xff0d); time.sleep(3); screenshot('02-editor-open')
 (output/'ready.json').write_text(json.dumps({'window':win,'geometry':verified_size,'windows':windows()}))
 seen=set(); deadline=time.monotonic()+600
 while time.monotonic()<deadline:
  if p.poll() is not None: break
  for f in sorted(commands.glob('*.json')):
   if f.name in seen: continue
   command=json.loads(f.read_text()); seen.add(f.name)
   if 'close_title' in command:
    matches=[w for w,t in windows() if command['close_title'] in t]
    if not matches: raise RuntimeError('Close title missing: '+command['close_title'])
    class Data(C.Union): _fields_=[('b',C.c_char*20),('s',C.c_short*10),('l',C.c_long*5)]
    class Client(C.Structure): _fields_=[('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),('display',C.c_void_p),('window',C.c_ulong),('message_type',C.c_ulong),('format',C.c_int),('data',Data)]
    class Event(C.Union): _fields_=[('client',Client),('pad',C.c_long*24)]
    x.XInternAtom.argtypes=[C.c_void_p,C.c_char_p,C.c_int]; x.XInternAtom.restype=C.c_ulong
    event=Event(); event.client.type=33; event.client.window=matches[0]; event.client.message_type=x.XInternAtom(d,b'WM_PROTOCOLS',0); event.client.format=32
    event.client.data.l[0]=x.XInternAtom(d,b'WM_DELETE_WINDOW',0); event.client.data.l[1]=0
    x.XSendEvent.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_long,C.POINTER(Event)]
    x.XSendEvent(d,matches[0],0,0,C.byref(event)); x.XFlush(d)
   if 'focus_title' in command:
    matches=[w for w,t in windows() if command['focus_title'] in t]
    if not matches: raise RuntimeError('Focus title missing: '+command['focus_title'])
    x.XSetInputFocus(d,matches[0],2,0); x.XFlush(d)
   if 'click' in command: click(*command['click'])
   if 'key' in command: key(command['key'])
   if 'text' in command: text(command['text'])
   time.sleep(command.get('wait',1))
   if 'capture' in command: screenshot(command['capture'])
   if command.get('finish'):
    x.XSetInputFocus(d,win,2,0)
    ctrl=x.XKeysymToKeycode(d,0xffe3); q=x.XKeysymToKeycode(d,ord('q'))
    for kc,down in [(ctrl,1),(q,1),(q,0),(ctrl,0)]: xt.XTestFakeKeyEvent(d,kc,down,0)
    x.XFlush(d); code=p.wait(timeout=30)
    (output/'result.json').write_text(json.dumps({'exit_code':code,'geometry':verified_size,'painter_canvas_ui':True,'commands':sorted(seen)},indent=2))
    break
  if p.poll() is not None: break
  time.sleep(.25)
finally:
 if p.poll() is None: p.terminate(); p.wait(timeout=10)
 log.close()

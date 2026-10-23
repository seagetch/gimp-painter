#!/usr/bin/env python3
import importlib.util,json,pathlib,shutil,tempfile
P=pathlib.Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('validator',P/'validate.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
cases={'strict-critical':('fixed/stdout.log',lambda t:t.replace('gdk=0','gdk=1')),'offscreen-not-tested':('grab-controls.log',lambda t:t.replace('PASS offscreen','SKIP offscreen')),'no-external-pointer':('atspi-server.log',lambda t:t.replace('POINTER 1','POINTER 0')),'anonymous-bus':('dbus-loopback.conf',lambda t:t.replace('DBUS_COOKIE_SHA1','ANONYMOUS')),'wrong-loaded-runtime':('gimp-integration/loaded-gtk-maps.txt',lambda t:t.replace('/gtk-menu-guards/prefix/','/old-gtk/prefix/')),'wrong-license':('license-COPYING',lambda t:t+'\nchanged\n'),'failed-ui':('gimp-integration/results.json',lambda t:t.replace('"result": "OK"','"result": "FAIL"'))}
result=[]
for name,(file,change) in cases.items():
 with tempfile.TemporaryDirectory(prefix='gtk-guard-validator-') as d:
  p=pathlib.Path(d)/'evidence';shutil.copytree(P,p);f=p/file;f.write_text(change(f.read_text()))
  try:m.check(p)
  except AssertionError:result.append({'case':name,'rejected':True})
  else:raise SystemExit('Mutation accepted: '+name)
(P/'validator-controls.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS7 independent receipt mutations rejected')

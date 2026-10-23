#!/usr/bin/env python3
"""Validate bounded tile-icon receipts; no tests are rerun here."""
import argparse, hashlib, json, re, struct
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args()
def read(n):return json.loads((HERE/n).read_text())
def need(x,m):
 if not x:raise SystemExit('FAIL: '+m)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def tap(text,n):
 need(f'1..{n}' in text.splitlines(),'TAP plan');need(sum(x.startswith('ok ') for x in text.splitlines())==n,'TAP pass count')
 need(not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',text),'no skipped/TODO/failed cases')
r=read('red.json')['tests'];need(len(r)==1,'one expected red');r=r[0]
need(r['result']=='FAIL' and r['returncode']==-6 and "'dark' should be TRUE" in r['stdout']+r['stderr'],'missing-glyph expected SIGABRT')
need(r['command'][-2:]==['-p','/painter-icons/layer-missing-name'],'actual layer baseline case')
g=read('green.json')['tests'];need(len(g)==1,'one focused native target');g=g[0]
need(g['result']=='OK' and g['returncode']==0 and g['duration']<60,'focused native pass');tap(g['stdout'],5)
for name in ('layer-missing-name','layer-loader-null','layer-render-null','tool-missing-name','tool-null-name'):need('/painter-icons/'+name in g['stdout'],'case '+name)
expected={'painter-layer-ui':(70,180),'painter-canvas-ui':(107,60)}
rows=read('aggregate.json')['tests'];need(len(rows)==2,'two unchanged aggregates')
for row in rows:
 name=row['name'].rsplit(' / ',1)[-1];need(name in expected,'known aggregate');count,limit=expected[name]
 need(row['result']=='OK' and row['returncode']==0 and row['duration']<limit,'aggregate pass/budget');tap(row['stdout'],count)
registered=read('registered-targets.json');need({n:v['timeout'] for n,v in registered.items()}=={'painter-layer-ui':180,'painter-canvas-ui':60,'painter-icon-fallback':60},'unchanged registered budgets')
a=read('asan-results.json');need(a['exit_code']==0 and a['tap_ok']==5 and a['timeout_seconds']==60 and a['duration_seconds']<60,'focused sanitizer pass/budget');tap((HERE/'asan.stdout').read_text(),5)
need(a['whole_application_instrumented'] is False and a['leak_detection'] is False,'bounded sanitizer claim');need(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',(HERE/'asan.stderr').read_text()),'no sanitizer errors')
b=read('asan-build.json');need(not b['changed_during_build'],'no build source drift');need(b['executables']['painter-icon-fallback']['sha256']==a['executable_sha256'],'tested executable hash')
for name in ('app/widgets/gimppainterlayertiles.cpp','app/display/gimppaintercanvasui.cpp','app/tests/test-painter-icon-fallback.c'):need(name in b['instrumented_sources'],'required instrumentation '+name)
for name in ('red-run.log','green-run.log','aggregate-run.log','asan-run.log'):need('1080x1920x24, authenticated accepted, empty authority rejected' in (HERE/name).read_text(),'authenticated portrait '+name)
u=read('ui-result.json');need(u['screen']==[1080,1920,24] and u['oncanvas_enabled'] and u['pixels_inspected'],'oncanvas/portrait visually inspected')
image=(HERE/'portrait-oncanvas-missing-icon.png').read_bytes();need(image[:8]==b'\x89PNG\r\n\x1a\n','PNG');need(list(struct.unpack('>II',image[16:24]))==u['window_capture']==[1040,1800],'portrait screenshot dimensions')
for mf,folder in [('source-sha256.json','tested-source'),('aggregate-checkpoint-sha256.json','tested-aggregate-checkpoint')]:
 for name,h in read(mf).items():
  need(sha(HERE/folder/name)==h,'tested source snapshot '+name)
  if mf=='source-sha256.json':
   need(b['sources_sha256'][name]==h,'instrumented tested source '+name)
   if args.check_source:need(sha(ROOT/name)==h,'current source '+name)
need('g_object_set (gimp->config,"painter-canvas-ui",TRUE,NULL)' in (HERE/'tested-source/app/tests/test-painter-icon-fallback.c').read_text(),'explicit oncanvas fixture')
need(sha(HERE/'red-gimppainterlayertiles.cpp')!=read('source-sha256.json')['app/widgets/gimppainterlayertiles.cpp'],'production delta from expected red')
m=read('evidence-sha256.json');need(set(m)=={str(f.relative_to(HERE)) for f in HERE.rglob('*') if f.is_file() and f.name!='evidence-sha256.json' and '__pycache__' not in f.parts},'complete artifact manifest')
for name,h in m.items():need(sha(HERE/name)==h,'artifact '+name)
print('PASS: expected red,5 native+5 focused sanitizer,177 aggregate cases,exact snapshots,portrait/oncanvas; full UI remains open')

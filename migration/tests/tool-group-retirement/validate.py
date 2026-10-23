#!/usr/bin/env python3
"""Validate bounded native receipts; does not rerun tests or claim whole-port parity."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args()
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
def read(n):return json.loads((HERE/n).read_text())
def require(ok,msg):
 if not ok:raise SystemExit('FAIL: '+msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def tap(s,n):
 require(f'1..{n}' in s.splitlines(),'exact TAP plan')
 require(sum(l.startswith('ok ') for l in s.splitlines())==n,'exact TAP passes')
 require(not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',s),'no skip/TODO/fail')
red=read('red.json')['tests'];require(len(red)==1,'one focused expected red');red=red[0]
require(red['result']=='FAIL' and red['returncode']==-5,'expected native SIGTRAP')
require(red['command'][-2:]==['-p','/perspective-ui/retained-groups-deserialize'],'exact red case')
require('gimp_tool_group_child_add' in red['stdout']+red['stderr'] and '== NULL' in red['stdout']+red['stderr'],'parent ownership assertion')
green=read('green.json')['tests'];require(len(green)==1,'one full normal run');green=green[0]
require(green['result']=='OK' and green['returncode']==0 and green['duration']<60,'native pass/original60s budget');tap(green['stdout'],24)
for name in ('retained-groups-deserialize','retained-groups-reset','old-toolrc-preserves-groups'):
 require('/perspective-ui/'+name in green['stdout'],'required regression '+name)
a=read('asan-results.json');require(a['exit_code']==0 and a['tap_ok']==24 and a['timeout_seconds']==60 and a['duration_seconds']<60,'full sanitizer24/original60s')
require(a['whole_application_instrumented'] is False and a['leak_detection'] is False,'sanitizer scope disclosure');tap((HERE/'asan.stdout').read_text(),24)
require(not re.search(r'AddressSanitizer:|LeakSanitizer:|runtime error:',(HERE/'asan.stderr').read_text()),'no sanitizer error')
b=read('asan-build.json');require(len(b['instrumented_sources'])==9 and not b['rtti_compatibility_only_sources'] and not b['changed_during_build'],'nine C units/no build drift')
require(b['executables']['painter-perspective-ui']['sha256']==a['executable_sha256'],'tested executable hash')
for n in ('red-run.log','green-run.log','asan-run.log'):
 require('1080x1920x24, authenticated accepted, empty authority rejected' in (HERE/n).read_text(),'authenticated portrait '+n)
u=read('ui-result.json');require(u['screen']==[1080,1920,24] and u['oncanvas_enabled'] and u['pixels_inspected'],'portrait/oncanvas observation')
img=(HERE/'portrait-oncanvas-after-reset.png').read_bytes();require(img[:8]==b'\x89PNG\r\n\x1a\n','PNG');require(list(struct.unpack('>II',img[16:24]))==u['actual_window_capture']==[1040,1800],'actual portrait window')
for manifest,folder in [('source-sha256.json','tested-source'),('factory-checkpoint-sha256.json','tested-factory-checkpoint')]:
 for name,h in read(manifest).items():
  require(sha(HERE/folder/name)==h,'immutable tested source '+name)
  if args.check_source:require(sha(ROOT/name)==h,'current source '+name)
require(sha(HERE/'red-gimp-tools.c')!=read('source-sha256.json')['app/tools/gimp-tools.c'],'production fix differs from red baseline')
require('g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL)' in (HERE/'tested-source/app/tests/test-painter-perspective-ui.c').read_text(),'explicit oncanvas not removed')
m=read('evidence-sha256.json');require(set(m)=={str(f.relative_to(HERE)) for f in HERE.rglob('*') if f.is_file() and f.name!='evidence-sha256.json' and '__pycache__' not in f.parts},'complete artifact manifest')
for name,h in m.items():require(sha(HERE/name)==h,'artifact '+name)
print('PASS: expected red, full24 normal+bounded sanitizer, source/checkpoint hashes, portrait/oncanvas; full UI parent remains open')

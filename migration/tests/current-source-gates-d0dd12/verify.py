#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,hashlib
P=Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for p,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
m=json.loads((P/'source-manifest.json').read_text());tree={}
for e in subprocess.check_output(['git','ls-tree','-rz',m['commit']],cwd=R).split(b'\0'):
 if not e:continue
 meta,p=e.split(b'\t',1);mode,kind,h=meta.decode().split();p=p.decode()
 if kind=='blob'and(p.startswith(('app/','libgimp','pdb/','tools/','migration/'))or p in ['tasks.md','meson.build']):tree[p]=h
assert tree=={p:r['git_blob']for p,r in m['files'].items()}
proc=subprocess.Popen(['git','cat-file','--batch'],cwd=R,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
for p,r in m['files'].items():
 proc.stdin.write((r['git_blob']+'\n').encode());proc.stdin.flush();h=proc.stdout.readline().split();b=proc.stdout.read(int(h[2]));assert proc.stdout.read(1)==b'\n';assert sha(b)==r['sha256'],p
proc.stdin.close();assert proc.wait()==0
assert json.loads((P/'source-after.json').read_text())=={'commit':m['commit'],'checked':len(tree),'changed':[]}
for n in ['adapter','persistence']:assert (P/(n+'.status')).read_text().strip()=='0'
assert 'PASS: 2535 sources; 720 reviewed data sites; 4 dormant legacy findings' in(P/'adapter.log').read_text()
assert 'PASS: 1379 source-checked contract records; 873 exact candidate dispositions;16 routes' in(P/'persistence.log').read_text()
assert all(not r['linked']for p,r in m['files'].items()if p.startswith('app/'))
print('PASS exact d0dd Git-only16840 inputs; adapter2535/720/4 and persistence1379/873/16; source-only checkpoint')

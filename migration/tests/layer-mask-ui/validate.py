#!/usr/bin/env python3
"""Verify raw portrait captures and actual native fixture results."""
import argparse,hashlib,json,pathlib,re,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
def no_env(value):
 if isinstance(value,dict):
  assert 'env' not in value
  for child in value.values():no_env(child)
 elif isinstance(value,list):
  for child in value:no_env(child)
for path in here.glob('*.json'):no_env(json.loads(path.read_text()))
m=json.loads((here/'report.json').read_text())
for name,sha in m['evidence_sha256'].items():
 assert hashlib.sha256((here/name).read_bytes()).hexdigest()==sha,name
for name in ('mask-applied.png','mask-disabled.png'):
 data=(here/name).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'
 assert struct.unpack('>II',data[16:24])==(1080,1920) and len(data)>10000
assert (here/'mask-applied.png').read_bytes()!=(here/'mask-disabled.png').read_bytes()
rows=json.loads((here/'visual.json').read_text());assert len(rows)==1
r=rows[0];assert r['result']=='OK' and r['returncode']==0 and r['duration']<120
assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==1
stages=re.findall(r'RAW_CAPTURE (mask-\w+.png) width=1080 height=1920 painter-canvas-ui=1 apply=(\d) left_alpha=1 right_alpha=(\d) raw_and_mask_unchanged=1',r['stdout'])
assert stages==[('mask-applied.png','1','0'),('mask-disabled.png','0','1'),('mask-applied.png','1','0')],stages
rows=json.loads((here/'regression.json').read_text());assert len(rows)==2
for r in rows:
 name=r['name'].split(' / ')[-1];counts={'painter-mypaint-tool':24,'gimp-painter-surface':18}
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<(120 if name=='painter-mypaint-tool' else 60)
 assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==counts[name] and '# SKIP' not in r['stdout']
 if name=='painter-mypaint-tool':
  for case in ('raw-apply-parity','projection-flags','tool-target-undo-cancel'):assert '/painter-tool/layer-mask/'+case in r['stdout']
  assert '/visual-capture' not in r['stdout']
if a.check_source:
 for path,sha in m['source_sha256'].items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('Actual raw portrait captures: applied/disabled/reapplied, oncanvas enabled, native pixel oracle;42 original regression cases PASS')

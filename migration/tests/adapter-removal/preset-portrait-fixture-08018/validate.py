#!/usr/bin/env python3
import argparse,json,pathlib,hashlib,re,tarfile,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];read=lambda n:json.loads((E/n).read_text())
rows=read('normal-results.json');assert len(rows)==2
for row in rows:
 assert row['returncode']==0
 assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==(16 if 'layer-presets-ui' in row['name'] else 24)
red=read('expected-red-portrait-results.json');assert any(x['returncode']!=0 and 'native-primary-click' in x['stdout'] for x in red)
with tarfile.open(E/'tested-ui-overlay.tar.gz') as tar:
 for row in read('source-manifest.json'):
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source and row['path']=='app/tests/test-layer-presets-ui.c':assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256']
for name in ['native-preset-click-oncanvas-portrait.png','preset-clean-oncanvas-portrait.png']:
 data=(E/name).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack('>II',data[16:24])==(1040,1800)
text=(E/'run.log').read_text();assert '1080x1920x24' in text and 'empty authority rejected' in text
print('PASS: clean native preset UI16 + perspective24; authenticated portrait/oncanvas images; preserved failures; fixture source identity')

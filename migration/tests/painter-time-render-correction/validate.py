#!/usr/bin/env python3
import gzip,hashlib,json,pathlib
P=pathlib.Path(__file__).resolve().parent
EXPECTED='919b96d41d682f94b7038fe52e58489fa437fa371366ce8aa5659f564aededcf'
BAD='migration/tests/painter-time-sequences/timestamp-wrap-oncanvas-portrait.png'
def validate(root=None,p=None):
 p=p or P;root=root or P.parents[2];sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
 for n,h in json.loads((p/'artifacts.json').read_text()).items():assert sha(p/n)==h,n
 preserved=json.loads((p/'preserved-b804-files.json').read_text());assert len(preserved)>=20
 assert preserved[BAD]=='01fce03ca824e5c172f0a5e17e40a34c8d425377799a2004db0acece22976dbd'
 for n,h in preserved.items():assert sha(root/n)==h,n
 for mode in ['normal','sanitizer']:
  r=json.loads((p/(mode+'-receipt.json')).read_text());assert r['returncode']==0 and r['runtime_unchanged'],mode
  assert 'ok 1 /timestamp-render/wrap' in r['stdout']
  assert 'VERIFIED_RENDER target_active=1 canvas_mapped=1 oncanvas=1 root=1080x1920 window=1080x1920 editor_button=108,1813 sample=412,851 rgb=218,55,55' in r['stdout']
  assert list(r['screenshots'].values())==[EXPECTED] and sha(p/(mode+'-render.png'))==EXPECTED,mode
  assert 'runtime error:' not in r['stderr'] and 'ERROR: AddressSanitizer' not in r['stderr']
  b=json.loads(gzip.decompress((p/(mode+'-build.json.gz')).read_bytes()));assert b['unchanged'] and b['host']=='0a775df4135052a2c851c9a5dfa37cea25887268'
  assert b['source_inputs']['/workspace/shared/painter-time-render-correction/test-time-sequences.cpp']==sha(p/'test-time-sequences.cpp')
 assert json.loads((p/'window-declarations.json').read_text())['returncode']==0
 assert 'visual claim was invalid' in (p/'README.md').read_text()
if __name__=='__main__':validate();print('PASS: separate render1+1, final PNG capture hashes/visible pixel and preserved b804 evidence')

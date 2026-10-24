#!/usr/bin/env python3
"""Receipt/source validation; not original-task or platform completion."""
import gzip,hashlib,json,pathlib,re
P=pathlib.Path(__file__).resolve().parent
NAMES=['ordinary','same-time','reverse-resume','wrap','half-range-reject','zero-start','half-range-accepted']
def validate(root=None,p=None):
 p=p or P;root=root or P.parents[2]
 for name,digest in json.loads((p/'artifacts.json').read_text()).items():
  assert hashlib.sha256((p/name).read_bytes()).hexdigest()==digest,name
 for name,digest in json.loads((p/'current-source-pins.json').read_text()).items():
  assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest,name
 for name in ['final-normal','final-sanitizer']:
  r=json.loads((p/(name+'.json')).read_text());assert r['returncode']==0,name
  assert r['runtime_inputs_unchanged'],name
  assert re.findall(r'^ok \d+ /timestamp-sequence/(\S+)',r['stdout'],re.M)==NAMES,name
  assert 'runtime error:' not in r['stderr'] and 'ERROR: AddressSanitizer' not in r['stderr'],name
  assert len(re.findall(r'painted alpha pixels=(\d+)',r['stdout']))==14,name
  assert 'Actual oncanvas TRUE/get_visible;1080x1920 root' in r['stdout'],name
 for name,assertion in [('reverse-mutant','sample.seconds'),('boundary-mutant','sample.seconds'),('equal-mutant','delivered.size()')]:
  r=json.loads((p/(name+'.json')).read_text());assert r['returncode']!=0,name
  assert r['runtime_inputs_unchanged'] and assertion in r['stdout']+r['stderr'],name
 landscape=json.loads((p/'landscape-normal.json').read_text());reject=json.loads((p/'landscape-reject.json').read_text())
 assert landscape['returncode']==0 and 'ok 1 /timestamp-sequence/ordinary' in landscape['stdout']
 assert reject['returncode']!=0 and '(1920 == 1080)' in reject['stdout']+reject['stderr']
 assert landscape['runtime_inputs_unchanged'] and reject['runtime_inputs_unchanged']
 for name in ['final-normal-build','sanitizer-build','reverse-mutant-build','boundary-mutant-build','equal-mutant-build']:
  b=json.loads(gzip.decompress((p/(name+'.json.gz')).read_bytes()));assert b['host']=='0a775df4135052a2c851c9a5dfa37cea25887268' and b['unchanged'],name
  assert b['source_inputs']['/workspace/shared/painter-time-sequences/test-time-sequences.cpp']==json.loads((p/'current-source-pins.json').read_text())['app/tests/test-painter-time-sequences.cpp'],name
 readme=(p/'README.md').read_text();assert 'OPEN/DOING' in readme and 'not queued GDK' in readme and 'No production implementation changed' in readme
if __name__=='__main__':
 validate();print('PASS: timestamp7+7, three causal mutations, exact source/receipts; original23.009 remains open')

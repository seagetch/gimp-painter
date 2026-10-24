#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,sys,subprocess
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for p,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/p).read_bytes())==h,p
r=json.loads((E/'results.json').read_text());assert not r['changed']
with tarfile.open(E/'sources.tar.gz')as t:
 assert set(t.getnames())==set(r['inputs'])
 for p,h in r['inputs'].items():
  assert sha(t.extractfile(p).read())==h,p
  if '--check-source'in sys.argv:assert sha((R/p).read_bytes())==h,p
assert len(r['instrumented_units'])==3 and set(r['runs'])=={'normal','sanitizer'}
for row in r['runs'].values():assert row['returncode']==0 and 'PASS 75 raster/fault/empty/unsupported cases'in row['stdout']
assert '-fsanitize=address,undefined'in r['runs']['sanitizer']['command'] and '-fno-rtti'in r['runs']['sanitizer']['command']
assert len(r['mutants'])==3
for row in r['mutants']:assert row['returncode']!=0 and 'Assertion'in row['stderr']
for p,h in json.loads((E/'predecessor.json').read_text()).items():assert sha((R/p).read_bytes())==h,p
subprocess.run([sys.executable,'-B',str(E.parent/'normalize-kernel/validate.py')],check=True)
print('PASS compact Normalize raw transport75 normal+three-unit sanitizer cases,3 rejected mutants; no native/ROI publication claim')

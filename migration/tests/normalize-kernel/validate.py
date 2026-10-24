#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,sys
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for p,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/p).read_bytes())==h,p
r=json.loads((E/'results.json').read_text());assert not r['changed']
assert sha((E/'filter-normalize-kernel.hpp').read_bytes())==r['inputs']['app/painter/filter-normalize-kernel.hpp']
for p,h in r['inputs'].items():
 if '--check-source' in sys.argv:assert sha((R/p).read_bytes())==h,p
assert set(r['runs'])=={'normal','sanitizer'}
for row in r['runs'].values():assert row['returncode']==0 and 'PASS 11316224 defined pixel/alpha checks;'in row['stdout']
assert '-fsanitize=address,undefined'in r['runs']['sanitizer']['command']
assert '-std=c++14'in r['runs']['normal']['command'] and '-fno-rtti'in r['runs']['normal']['command']
assert len(r['mutants'])==3
for row in r['mutants']:assert row['returncode']!=0 and 'Assertion'in row['stderr']
for name in ['normalize-foundation','normalize-indexed-foundation']:
 for p,h in json.loads((E/(name+'-predecessor.json')).read_text()).items():assert sha((R/p).read_bytes())==h,p
 subprocess.run([sys.executable,'-B',str(E.parent/name/'validate.py')],check=True)
print('PASS Normalize immutable scalar kernel:11316224 normal+sanitizer,3 rejected mutants; native admission not implemented')

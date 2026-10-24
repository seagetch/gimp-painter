#!/usr/bin/env python3
import hashlib,json,pathlib,re
P=pathlib.Path(__file__).resolve().parent
R=P.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(p=P):
 manifest=json.loads((p/'manifest.json').read_text())
 for n,h in manifest.items():assert sha(p/n)==h,n
 seal=json.loads((p/'runtime-source-seal.json').read_text())
 old=json.loads((R/'migration/tests/perspective-display-compatibility/final/source-before-build.json').read_text())
 assert [n for n,h in old.items() if seal['source_sha256'][n]!=h]==['app/tests/meson.build']
 for build,mode in [('build','normal'),('build-asan','sanitizer')]:
  prior=json.loads((R/f'migration/tests/perspective-display-compatibility/final/{mode}-geometry.json').read_text())
  assert seal['binaries'][build]==prior[0]['executable_sha256']
  rows=json.loads((p/f'{build}.json').read_text());assert len(rows)==5
  names=set();cases=[]
  for row in rows:
   assert row['result']=='OK' and row['returncode']==0 and 0<row['duration']<60
   name=row['name'].split(' / ')[-1];names.add(name)
   actual=[x for x in row['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   assert actual==row['cases'];cases.extend(x.split(' ',2)[2] for x in actual)
   assert len(actual)==(24 if name=='painter-perspective-ui' else 2 if name.endswith('-geometry') else 1)
  assert names=={'painter-perspective-ui','painter-perspective-ui-geometry',*[f'painter-perspective-ui-damage-{i}' for i in range(3)]}
  assert len(cases)==29 and len(set(cases))==29
  original=json.loads((R/f'migration/tests/perspective-display-compatibility/final/{mode}-original.json').read_text())
  original_ui=next(r for r in original if r['name'].endswith(' / painter-perspective-ui'))
  expected={x.split(' ',2)[2] for x in original_ui['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x}
  expected.update(['/perspective-ui/damage-ink-bounds','/perspective-ui/old-path-pixels',*[f'/perspective-ui/damage-mutations/{i}' for i in range(3)]])
  assert set(cases)==expected
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/f'{build}.log').read_text()
 meson=(p/'meson.build').read_text();assert "args: scope[1]" in meson and "timeout: 60, is_parallel: false" in meson
 assert "test_args = ['--skip-prefix', '/perspective-ui/damage'," in meson
 assert "'--skip-prefix', '/perspective-ui/old-path']" in meson
 print('PASS: normal and ordinary sanitizer registered disjoint 29-case UI union, each scope <60 seconds')
if __name__=='__main__':check()

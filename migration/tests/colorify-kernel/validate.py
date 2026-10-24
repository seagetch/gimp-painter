#!/usr/bin/env python3
import hashlib,json,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent
def sha(b):return hashlib.sha256(b).hexdigest()
a=json.loads((P/'artifact-sha256.json').read_text());assert set(a)=={p.name for p in P.iterdir() if p.is_file() and p.name!='artifact-sha256.json'}
for p,h in a.items():assert sha((P/p).read_bytes())==h,p
j=json.loads((P/'results.json').read_text());assert not j['changed_after_run']
assert set(j['runs'])=={'normal-matrix','normal-contracts','sanitizer-matrix','sanitizer-contracts'}
for name,r in j['runs'].items():
 assert r['exit_code']==0 and not r['stderr']
 assert ('268435456 complete RGB/RGBA pixel comparisons;98685 binary64 tint-boundary comparisons' if name.endswith('matrix') else '20 invalidRGBA+3 rounding rejections;9 invalid pixel/index calls;8 separate/exact/partial aliases') in r['stdout']
 if name.startswith('sanitizer'):assert '-fsanitize=address,undefined,float-cast-overflow' in r['compile']
with tarfile.open(P/'sources.tar.gz') as t:
 names=t.getnames();assert len(names)==len(set(names)) and set(names)==set(j['source_sha256'])
 for name,h in j['source_sha256'].items():assert not Path(name).is_absolute() and '..' not in Path(name).parts and sha(t.extractfile(name).read())==h
probe=json.loads((P/'header-probe.json').read_text());assert probe['exit_code']==0 and not probe['stderr'];assert probe['header_sha256']==j['source_sha256']['app/painter/filter-colorify-kernel.hpp']
native=json.loads((P/'header-native14-probes.json').read_text());assert native['header_sha256']==probe['header_sha256'];assert len(native['runs'])==2 and all(x['exit_code']==0 and not x['stderr'] and '-std=c++14' in x['command'] and '-fno-rtti' in x['command'] for x in native['runs'])
m=json.loads((P/'mutations.json').read_text());assert set(m)=={'combined-luminance','ignored-rounding-precondition'}
assert all(r['exit_code']!=0 for r in m.values());assert 'mismatch' in m['combined-luminance']['stderr'];assert 'Assertion' in m['ignored-rounding-precondition']['stderr']
print('PASS isolated Colorify normal/san268435456+98685;23 domain/rounding,9invalid,8aliases;2 production mutants;source seals;no native/storage route')

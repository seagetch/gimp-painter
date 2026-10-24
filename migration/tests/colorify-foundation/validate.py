#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
P=Path(__file__).resolve().parent
def sha(b):return hashlib.sha256(b).hexdigest()
a=json.loads((P/'artifact-sha256.json').read_text());assert set(a)=={p.name for p in P.iterdir() if p.is_file() and p.name!='artifact-sha256.json'}
for p,h in a.items():assert sha((P/p).read_bytes())==h,p
j=json.loads((P/'results.json').read_text());old=json.loads((P/'old-source.json').read_text());c=(P/'old-oracle.c').read_text()
assert old['pixel_function'] in c and old['lookup_initialization'] in c
for p,h in j['source_sha256'].items():assert sha((P/p).read_bytes())==h,p
assert set(j['runs'])=={'normal','sanitizer','rounded-tint','combined-luminance'}
for key in ['normal','sanitizer']:
 r=j['runs'][key];assert r['exit_code']==0 and not r['stderr'];assert '268435456 complete RGB/RGBA pixel comparisons;98685 binary64 tint-boundary comparisons;white254;alpha/hidden RGB preserved' in r['stdout']
assert '-fsanitize=address,undefined,float-cast-overflow' in j['runs']['sanitizer']['command']
for key in ['rounded-tint','combined-luminance']:assert j['runs'][key]['exit_code']!=0 and 'mismatch' in j['runs'][key]['stderr']
assert 'FE_TONEAREST' in (P/'compare.c').read_text()
print('PASS normal/san268435456 pixels+98685 tint boundaries each;2 causal mutants;exact old source and independent IEEE oracle;no native route claim')

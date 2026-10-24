#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,struct
P=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def result(j):
 assert j['exit_code']==0 and not j['stderr']
 rows=[x.split()for x in j['stdout'].splitlines()if x.startswith('WIRE ')];assert len(rows)==256
 name=b'plug-in-exchange\0';frame=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,(-123)&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b''.join(struct.pack('>II',4,1)+b'\0'for _ in range(9))+struct.pack('>II',0,0)
 assert len(frame)==138
 for k,row in enumerate(rows):assert int(row[1])==k and bytes.fromhex(row[2])==frame
 assert 'HOST GLib 2.84.4 INT8 UINT-derived, signed getter criticals=2304;256 frames,2304 channel values'in j['stdout']
a=json.loads((P/'artifact-sha256.json').read_text());assert set(a)=={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts}
for n,h in a.items():assert sha((P/n).read_bytes())==h,n
r=json.loads((P/'results.json').read_text());source=(P/'writer-oracle.c').read_text();assert sha(source.encode())==r['source_sha256']
ref=json.loads((P/'reference.json').read_text());assert ref['old_ref']=='afa43fae3e920210146abed514f136fd49f671b5'and len(ref['exact_functions'])==7
for n,body in ref['exact_functions'].items():assert source.count(body)==1,n
assert set(r['runs'])=={'normal','sanitizer'}
for j in r['runs'].values():result(j)
assert '-fsanitize=address,undefined'in r['runs']['sanitizer']['compile']
m=json.loads((P/'mutations.json').read_text());assert {x['name']for x in m}=={'unsigned-getter','wrong-int8-parent'}
for x in m:assert x['exit_code']!=0 and x['source_sha256']==r['source_sha256']
j=r['runs']['normal']
for bad in [dict(j,exit_code=1),dict(j,stdout=j['stdout'].replace('WIRE 0 ','DROP 0 ',1)),dict(j,stdout=j['stdout'].replace('criticals=2304','criticals=0'))]:
 try:result(bad)
 except AssertionError:pass
 else:raise AssertionError('negative result admitted')
print('PASS literal old INT8 writer/type on GLib2.84.4: normal+ASan256 frames/2304 expected criticals each, zero encodedbytes;2 mutants+3 controls; no lost-intent reconstruction')

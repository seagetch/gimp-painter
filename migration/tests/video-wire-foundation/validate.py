#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,struct
D=Path(__file__).resolve().parent;h=lambda b:hashlib.sha256(b).hexdigest()
for p,w in json.loads((D/'artifacts.json').read_text()).items():assert h((D/p).read_bytes())==w,p
s=(D/'writer-oracle.c').read_text();meta=json.loads((D/'pinned-writer-reference.json').read_text());assert meta['old_ref']=='afa43fae3e920210146abed514f136fd49f671b5'
for n,b in meta['exact_functions'].items():assert b in s,n
def check(row):
 name=b'plug-in-video\0';ints=[row[k]for k in ['run','pattern','additive','rotated']]
 want=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,ints[0]&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b''.join(struct.pack('>III',2,4,x&0xffffffff)for x in ints[1:])+b'\0'*8
 assert len(want)==90 and want.hex()==row['hex']
r=json.loads((D/'results.json').read_text());assert set(r)=={'normal','sanitizer'}
for mode,v in r.items():
 assert v['exit_code']==0 and len(v['cases'])==7 and not v['stderr']
 assert ('-fsanitize=address,undefined'in v['command'])==(mode=='sanitizer')
 for row in v['cases']:check(row)
assert r['normal']['cases']==r['sanitizer']['cases']
good=r['normal']['cases'][0]
for text in [good['hex'][:-2],'ff'+good['hex'][2:],good['hex'][:52]+'00000000'+good['hex'][60:]]:
 try:check(dict(good,hex=text))
 except AssertionError:pass
 else:raise AssertionError('bad frame accepted')
print('PASS Video seven90-byte exact writer cases normal/sanitizer;three independent frame controls;no old runtime')

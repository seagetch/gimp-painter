#!/usr/bin/env python3
"""Static source/asset inventory. This is not a runtime construction test."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];old=root.parent/'gimp-painter-legacy'
commit='afa43fae3e920210146abed514f136fd49f671b5'
expected=sorted(subprocess.check_output(['git','-C',str(old),'ls-tree','--name-only',commit,'data/layer-presets/'],text=True).splitlines())
expected=[x for x in expected if x.endswith('.json')]
fields={}
def walk(value,path='$'):
 kind='null' if value is None else 'boolean' if isinstance(value,bool) else 'object' if isinstance(value,dict) else 'array' if isinstance(value,list) else 'string' if isinstance(value,str) else 'integer' if isinstance(value,int) else 'number'
 fields.setdefault(path,set()).add(kind)
 if isinstance(value,dict):
  for key,item in value.items():walk(item,path+'.'+key)
 elif isinstance(value,list):
  for item in value:walk(item,path+'[]')
files=[]
for path in expected:
 source=subprocess.check_output(['git','-C',str(old),'show',commit+':'+path]);new=(root/path).read_bytes()
 assert source==new,path+' differs from pinned asset'
 value=json.loads(new);walk(value)
 files.append({'path':path,'name':value['name'],'sha256':hashlib.sha256(new).hexdigest(),'bytes':len(new)})
assert len(files)==8
report={'kind':'static exhaustive bundled JSON audit; not runtime parity evidence','legacy_commit':commit,'asset_count':len(files),'assets':files,'field_types':{k:sorted(v) for k,v in sorted(fields.items())}}
(root/'migration/fixtures/legacy-layer-presets/assets.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
print('8 pinned assets byte-identical;',len(fields),'observed JSON paths inventoried')

#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,tarfile,re
E=Path(__file__).resolve().parent;sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
j=json.loads((E/'old-source.json').read_text());assert j['ref']=='afa43fae3e920210146abed514f136fd49f671b5'
with tarfile.open(E/'old-sources.tar.gz') as t:
 assert set(t.getnames())==set(j['source_sha256'])
 for n,h in j['source_sha256'].items():assert sha(t.extractfile(n).read())==h,n
 v=t.extractfile('plug-ins/common/video.c').read().decode();it=t.extractfile('libgimp/gimpregioniterator.c').read().decode()
 for f in j['exact_fragments'].values():assert f in v and f in (E/'old-oracle.c').read_text()
 assert j['declared_arguments']in v and j['noninteractive_assignment']in v and j['coordinate_excerpt']in it
p=json.loads((E/'patterns.json').read_text());assert len(p['rows'])==9
rows=re.findall(r'\{([^{}]*)\}',j['exact_fragments']['table'])
for i,text in enumerate(rows):
 nums=list(map(int,re.findall(r'\d+',text)));assert len(nums)==p['widths'][i]*p['heights'][i]
 assert ''.join(map(str,nums))==''.join(p['rows'][i])
 for row in p['rows'][i]:assert '"'+row+'"'in (E/'reference-patterns.h').read_text()
r=json.loads((E/'results.json').read_text());assert not r['changed_after_run'] and set(r['runs'])=={'normal','sanitizer'}
for n,h in r['source_sha256'].items():assert sha((E/n).read_bytes())==h,n
for row in r['runs'].values():
 assert row['exit_code']==0 and not row['stderr']
 assert 'Video 13708800 independent'in row['stdout'] and 'rejected14'in row['stdout']
assert '-fsanitize=address,undefined'in r['runs']['sanitizer']['compile']
m=json.loads((E/'mutations.json').read_text());assert {x['name']for x in m}=={'reset-roi-phase','ignore-rotation','wrap-additive','erase-alpha'}
for x in m:assert x['returncode']!=0 and 'Video mismatch'in x['stderr'] and x['source_sha256']==r['source_sha256']['old-oracle.c']
print('PASS Video pinned source-only foundation:13708800 normal/sanitizer comparisons,14 harness-only guards,4 causal mutants; native route remains pending')

#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,gzip,tarfile,argparse,re
p=argparse.ArgumentParser();p.add_argument('--check-owned',action='store_true');a=p.parse_args();E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
f=json.loads((E/'frozen-manifest.json').read_text());snap=json.loads((E/'snapshot.json').read_text());h=json.loads((E/'source-sha256.json').read_text());assert len(h)==126 and snap['instrumented_count']==23 and snap['rtti_only_count']==51
manifests={n:json.loads(gzip.decompress((E/(n+'.json.gz')).read_bytes()))for n in ['red-source-before','red-source-after','green-source-before','green-source-after']}
for prefix in ['red','green']:
 b=manifests[prefix+'-source-before'];c=manifests[prefix+'-source-after'];assert b['sha256']==c['sha256']and not c['changed_after_run'];assert c['base_commit']=='503ade3466ddebc60d13d20502ebf0eb0c617f96'
g=manifests['green-source-after'];red=manifests['red-source-after'];assert {p for p in g['sha256']if g['sha256'][p]!=red['sha256'].get(p)}=={'app/core/gimpfilterlayer.cpp','migration/tests/run_colorify_native_sanitizers.py'}
assert 'app/painter/filter-video-kernel.hpp'not in g['sha256']
for n,v in f['sha256'].items():assert g['sha256'][n]==v
with tarfile.open(E/'sources.tar.gz')as t:
 assert set(t.getnames())==set(h)
 for n,v in h.items():assert sha(t.extractfile(n).read())==v==g['sha256'][n],n
for n,v in json.loads((E/'owned-sha256.json').read_text()).items():
 assert v==h[n]==f['sha256'][n]
 if a.check_owned:assert sha((R/n).read_bytes())==v,n
n=json.loads((E/'native.json').read_text());assert len(n)==14
for k,v in n.items():assert v['exit_code']==0 and '/gimp-filter-layer/'+k in v['stdout']
s=json.loads((E/'sanitizer.json').read_text());assert s['exit_code']==0 and not s['changed_after_compile'];assert len(s['instrumented_sources'])==23 and len(s['rtti_compatibility_only_sources'])==51;assert len(s['selected_test_paths'])==14
for path in s['selected_test_paths']:assert path in s['stdout']
assert len(re.findall(r'^ok \d+ ',s['stdout'],re.M))==14
for p,v in s['source_sha256'].items():assert h[p]==v
r=json.loads((E/'expected-red.json').read_text());assert r['exit_code']!=0 and 'unsupported'in(r['stdout']+r['stderr']).lower()
print('PASS native Colorify14normal+14selectedASan,23instrumented/51RTTI-only,126sourcearchive; exact503+frozen own overlays,Video raw excluded')

#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct
p=pathlib.Path(__file__).resolve().parent
for n,h in json.load(open(p/'artifact-sha256.json')).items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
r=json.load(open(p/'results.json'));assert len(r)==21 and all(x['result']=='OK'and x['returncode']==0 for x in r)
assert sum(len(re.findall(r'^ok\s+\d+',x.get('stdout',''),re.M))for x in r)==238
for n in ['postrun-source-seal.json','extra-ui-postrun-seal.json']:
 j=json.load(open(p/n));assert j['checked_files']==16265 and not j['changed']
x=json.load(open(p/'extra-ui-results.json'));assert len(x)==2 and all(y['returncode']==0 for y in x)
assert len(re.findall(r'^ok\s+\d+',x[0]['stdout'],re.M))==4
x=json.load(open(p/'graph-ui-results.json'));assert len(x)==1 and x[0]['returncode']==0 and len(re.findall(r'^ok\s+\d+',x[0]['stdout'],re.M))==3
j=json.load(open(p/'static-gate.json'));assert j['status']=='PASS'and len(j['sources'])==2521 and len(j['data_sites'])==718
images=list(p.rglob('*.png'));assert len(images)==14
for f in images:
 data=f.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';dims=struct.unpack('>II',data[16:24]);assert dims in [(1080,1920),(1040,1800)],(f,dims)
print('PASS: exact committed source seals; 21 targets / 238 cases + 4 Filter + 3 graph UI; 14 portrait screenshots')

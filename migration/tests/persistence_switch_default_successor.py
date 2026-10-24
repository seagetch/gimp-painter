"""One intentional declared-default field projection; immutable old family retained."""
import hashlib,io,json,pathlib,tarfile,re
PACKAGE='migration/tests/persistence-switch-default-successor'
OPTIONS='app/paint/painter-mypaint-surface/gimp-painter-options.cpp'
FAMILY='migration/inventory/persistence-mypaint-fields.json'
PINS={'sources.tar.gz':'b4801bc9628ae768c627b64acd714ece7e1cde10968c598fe41bb8aca86acffc','transition.json':'847fd8053174826840aa49c21ae4650dca09c518c28fb88248724021936a35e7'}
def sha(b):return hashlib.sha256(b).hexdigest()
def reviewed(root):
 root=pathlib.Path(root);b={n:(root/PACKAGE/n).read_bytes()for n in PINS}
 for n,h in PINS.items():assert sha(b[n])==h,('switch default evidence drift',n)
 r=json.loads(b['transition.json'])
 names=['predecessor/'+OPTIONS,'successor/'+OPTIONS,'predecessor/'+FAMILY,'successor/'+FAMILY,'predecessor/migration/inventory/persistence-fields.tsv']
 with tarfile.open(fileobj=io.BytesIO(b['sources.tar.gz']))as t:
  assert t.getnames()==names and all(m.isfile()for m in t.getmembers());a={n:t.extractfile(n).read()for n in names}
 old=a['predecessor/'+OPTIONS];new=a['successor/'+OPTIONS];needle=b'g_param_spec_boolean(s.internal_name,s.displayed_name,nullptr,FALSE,flags)'
 assert old.count(needle)==1 and old.replace(needle,needle.replace(b'FALSE',b's.default_value'))==new,'unreviewed class-init/source change'
 assert sha(old)==r['predecessor_sha256'] and sha(new)==r['successor_sha256']
 assert (root/OPTIONS).read_bytes()==new,'unreviewed current Options source drift'
 oldfamily=a['predecessor/'+FAMILY];newfamily=a['successor/'+FAMILY]
 assert sha(oldfamily)==r['family_predecessor_sha256'] and sha(newfamily)==r['family_successor_sha256']
 oldj=json.loads(oldfamily);newj=json.loads(newfamily);rows=[f for f in oldj['fields']if f['id']=='mypaint.option.use_gimp_brushmark'];assert len(rows)==1
 assert rows[0]['default']==r['default_before']=='Native GParamSpec default: FALSE; legacy GParamSpec metadata default: TRUE. Resource missing default remains FALSE.'
 assert r['default_after']=='Native GParamSpec default: TRUE; legacy GParamSpec metadata default: TRUE. Resource missing default remains FALSE.'
 rows[0]['default']=r['default_after'];assert oldj==newj,'neighbor fields/IDs/population/source seals changed'
 assert oldfamily.count(r['default_before'].encode())==1 and oldfamily.replace(r['default_before'].encode(),r['default_after'].encode())==newfamily
 assert (root/FAMILY).read_bytes()==newfamily,'unreviewed current family drift'
 runtime=root/r['runtime_package'];manifest=(runtime/'artifact-sha256.json').read_bytes();assert sha(manifest)==r['runtime_manifest_sha256']
 for p,h in json.loads(manifest).items():assert sha((runtime/p).read_bytes())==h,('default runtime evidence changed',p)
 m=json.loads((runtime/'source-manifest.json').read_text());assert m['owned'][OPTIONS]==r['successor_sha256']
 for name,n in [('full',28),('green',4),('asan',4),('numeric-baseline',3),('numeric-candidate',3),('ui',1)]:
  result=json.loads((runtime/name/'result.json').read_text());log=(runtime/name/'run.log').read_text();assert result['exit']==0 and len(re.findall(r'^ok \d+',log,re.M))==n and '# SKIP' not in log
 assert 'removing actual native painter-settings causally changes omitted FALSE to reset TRUE' in (runtime/'green/run.log').read_text()
 return r
def provider(root,path,predecessor,actual):
 r=reviewed(root);assert path==OPTIONS and predecessor==r['predecessor_sha256'] and actual==r['successor_sha256'],'wrong default transition endpoints';return r
def family_projection(root,old,current):
 r=reviewed(root);assert sha(old)==r['family_predecessor_sha256'] and sha(current)==r['family_successor_sha256'],'wrong family predecessor/current';return r
def property_predecessor(root,path,current):
 r=reviewed(root);assert path==OPTIONS and sha(current)==r['successor_sha256']
 with tarfile.open(pathlib.Path(root)/PACKAGE/'sources.tar.gz')as t:return t.extractfile('predecessor/'+OPTIONS).read()

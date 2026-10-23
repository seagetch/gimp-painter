#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,tarfile,re,argparse
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();d=Path(__file__).resolve().parent;r=d.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for f in ('results.json','asan-results.json'):
 rows=json.loads((d/f).read_text());assert len(rows)==3
 assert sorted(int(re.search(r'^1\.\.(\d+)$',x['stdout'],re.M)[1]) for x in rows)==[4,6,14]
 for x in rows:assert x['returncode']==0 and 'not ok' not in x['stdout']
for f,needle in [('mutant-missing-skip.json','gimp_get_data_factory'),('mutant-excluded-subtree.json','async->priv->stopped')]:
 x=json.loads((d/f).read_text())[0];assert x['returncode']!=0 and needle in x['stdout']
assert json.loads((d/'restored.json').read_text())[0]['returncode']==0
assert 'UNKNOWN FACTORY TYPE GimpJsonResource' in json.loads((d/'feature-diagnostic-failed.json').read_text())[0]['stdout']
b=json.loads((d/'asan-build.json').read_text());assert len(b['instrumented_sources'])==16 and not b['changed_during_build']
with tarfile.open(d/'sanitizer-tested-sources.tar.gz') as t:
 for f,h in b['sources_sha256'].items():
  assert sha(t.extractfile(f).read())==h
  if a.check_source:assert sha((r/f).read_bytes())==h,f
assert '1080x1920x24' in (d/'tests.log').read_text()
print('PASS readiness fix24normal/24selected16-unit sanitizer;2compiled mutants;real app persistence4;exact selected sources;not whole HEAD')

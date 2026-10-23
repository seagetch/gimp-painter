#!/usr/bin/env python3
"""Validate pinned native ownership reviews; optionally compare current sources."""
import argparse, collections, hashlib, json, pathlib, re, sys
HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import check_painter_legacy_adapters as gate
parser=argparse.ArgumentParser();parser.add_argument('--check-source',action='store_true');args=parser.parse_args()
review=json.loads((HERE/'review.json').read_text());assert review['status']=='VERIFIED_NATIVE_C_OWNERSHIP'
rows=review['rows'];assert len(rows)==6 and sum(r['count']for r in rows)==7
expected=collections.Counter({(r['path'],r['operation'],r['sha256']):r['count']for r in rows})
assert len(expected)==6
policy=json.loads((ROOT/'migration/contracts/legacy-adapter-removal.json').read_text())
for row in rows:
 matches=[p for p in policy['data_sites']if all(p[k]==row[k]for k in ['path','operation','sha256','count'])]
 assert len(matches)==1 and matches[0]['reason']
 assert row['path'] in policy['reviews'][matches[0]['provenance']]['paths']
for name,count in [('compact-normal.json',62),('session-normal.json',33)]:
 data=json.loads((HERE/name).read_text());assert len(data['tests'])==1
 test=data['tests'][0];assert test['result']=='OK' and test['returncode']==0
 paths=re.findall(r'^ok\s+\d+\s+(/[^\n]+)',test['stdout'],re.M)
 assert len(paths)==count and len(set(paths))==count
 assert not re.search(r'^not ok\b',test['stdout'],re.M)
 assert not re.search(r'^ok\s+\d+.*#\s*SKIP',test['stdout'],re.M|re.I)
 assert 'env'not in test
baseline=json.loads((HERE/'baseline-gate-summary.json').read_text());assert baseline['status']=='FAIL' and len(baseline['errors'])==6
final=json.loads((HERE/'final-gate-summary.json').read_text());assert final['status']=='PASS' and not final['errors']
if args.check_source:
 for path in {r['path']for r in rows}:
  source=(ROOT/path).read_bytes();assert all(hashlib.sha256(source).hexdigest()==r['source_sha256']for r in rows if r['path']==path)
  result=gate.inspect_source(source.decode(),path);assert not result['findings']
  actual=collections.Counter((x['path'],x['operation'],x['sha256'])for x in result['data_sites'])
  for key,n in expected.items():
   if key[0]==path:assert actual[key]==n
print('PASS:6 exact fingerprints/7native calls,95 native cases, unchanged gate semantics')

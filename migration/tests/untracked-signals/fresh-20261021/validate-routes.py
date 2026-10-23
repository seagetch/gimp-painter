#!/usr/bin/env python3
"""Check exact historical route accounting; never turn a draft into acceptance."""
import argparse,csv,hashlib,json,pathlib,re,subprocess
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[3]
p=argparse.ArgumentParser();p.add_argument('--allow-draft',action='store_true');args=p.parse_args()
review=json.loads((HERE/'route-review.json').read_text());rows=review['rows']
with (ROOT/'migration/inventory/signal-source-inventory.tsv').open()as f:
 expected={r['legacy_site']for r in csv.DictReader(f,delimiter='\t')if r['followup']=='07.008/untracked-signals'}
with (HERE.parent/'source-map.tsv').open()as f:
 original={r['legacy_site']:r for r in csv.DictReader(f,delimiter='\t')}
assert set(original)==expected
actual=[r['legacy_site']for r in rows];assert len(actual)==len(set(actual))==len(expected)==60 and set(actual)==expected
retired=json.loads((HERE/'retired-legacy-routes.json').read_text());assert len(retired['rows'])==3
retired_sites={r['legacy_site']for r in retired['rows']}
for r in retired['rows']:
 source=subprocess.check_output(['git','show',retired['legacy_commit']+':'+r['legacy_site'].rsplit(':',1)[0]],cwd=ROOT)
 assert hashlib.sha256(source).hexdigest()==r['source_sha256']
 start,end=r['range'];assert '\n'.join(source.decode().splitlines()[start-1:end])+'\n'==r['excerpt']
for r in rows:
 assert r['original_mapping']==original[r['legacy_site']], 'original mapping/criterion changed'
 assert r['original_mapping']['remaining_acceptance'] and r['disposition']
 assert bool(r['case_identifiers'])==(r['legacy_site']not in retired_sites)
 assert r['evidence_package']
test_sources='\n'.join(path.read_text(errors='replace')for path in (ROOT/'app/tests').iterdir()if path.is_file())
for row in rows:
 for identifier in row['case_identifiers']:
  assert identifier in test_sources, 'unregistered/missing native case: '+identifier
if not args.allow_draft:
 assert review['status']=='ACCEPTED_BOUNDED_OWNER_ROUTES', 'route review is still a draft'
 assert not review['pending']
 assert all(r['review_status']in ('NATIVE_OWNER_ROUTE_VERIFIED','PINNED_RETIRED_SOURCE_VERIFIED')for r in rows)
 assert review['acceptance_receipt'] and (ROOT/review['acceptance_receipt']).is_file()
 receipt=json.loads((ROOT/review['acceptance_receipt']).read_text())
 assert receipt['task']=='07.008/untracked-signals' and receipt['status']==review['status']
 assert (receipt['original_rows'],receipt['native_route_duties'],receipt['pinned_retired_routes'])==(60,57,3)
 subprocess.run(['python3',str(HERE/'validate-evidence.py')],check=True)
 passed=set()
 def tap(text):
  for line in text.splitlines():
   match=re.match(r'^ok \d+ (\S+)',line)
   if match:passed.add(match[1])
 def walk(value):
  if isinstance(value,dict):
   for key,item in value.items():
    if key=='stdout' and isinstance(item,str):tap(item)
    else:walk(item)
  elif isinstance(value,list):
   for item in value:walk(item)
 for package in (HERE,ROOT/'migration/tests/application-lifecycle-signals',ROOT/'migration/tests/editor-page-memory',ROOT/'migration/tests/vfunc-exception-containment/layer-tiles-initializer'):
  for path in package.rglob('*'):
   if not path.is_file():continue
   if path.suffix=='.json':walk(json.loads(path.read_text()))
   elif path.suffix in ('.log','.stdout'):tap(path.read_text(errors='replace'))
 for row in rows:
  for identifier in row['case_identifiers']:
   assert any(name.endswith('/'+identifier)for name in passed),'missing actual TAP pass: '+identifier
print('PASS:60 exact routes,3 pinned retired routes,57 native-route obligations; '+review['status'])

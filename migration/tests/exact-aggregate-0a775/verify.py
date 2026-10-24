#!/usr/bin/env python3
"""Read-only exact-source integration and historical counting admission."""
import hashlib,io,json,pathlib,re,struct,tarfile
COMMIT='0a775df4135052a2c851c9a5dfa37cea25887268'
ARCHIVE='14f5dfdd5defacde4632d1ff5ee1c31535e710eccbb2ea3b72022d5e61ad79b5'
def taps(text):
 lines=[x for x in (text or '').splitlines()if re.match(r'^ok \d+',x)]
 return [x for x in lines if '# SKIP'not in x.upper()],[x for x in lines if '# SKIP'in x.upper()]
def validate(rows,summary,extras,graph):
 assert len(rows)==47 and all(r['result']=='OK'for r in rows)
 assert sum(len(taps(r.get('stdout'))[0])for r in rows)==515
 assert sum(len(taps(r.get('stdout'))[1])for r in rows)==6
 event=next(r for r in rows if r['name'].endswith(' / event-compression-order'))
 done,skipped=taps(event['stdout']);assert len(done)==14 and len(skipped)==1 and 'close-active-image' in skipped[0]
 options=next(r for r in rows if r['name'].endswith(' / gimp-painter-options'));assert len(taps(options['stdout'])[0])==28 and not taps(options['stdout'])[1]
 expected={'colorify-ui':7,'red-eye-exchange-xcf':14,'red-eye-portrait':1,'filter-selected-no-edge':65,'graph-ui':3}
 assert len(extras)==5 and all(r['returncode']==0 for r in extras)
 for r in extras:
  output=graph[0]['stdout']if r['name']=='graph-ui'else r['stdout'];assert len(taps(output)[0])==expected[r['name']] and not taps(output)[1]
 assert len(graph)==1 and graph[0]['result']=='OK'
 assert summary['source_commit']==COMMIT and summary['registered_targets']==47 and summary['passed_targets']==47
 assert summary['completed_ok_tap_cases']==515 and summary['selected_extra_cases']==expected and summary['selected_extra_total']==90
 assert summary['whole_selected_aggregate_pass'] and len(summary['non_tap_targets'])==2
 assert {n.rsplit(' / ',1)[-1]for n in summary['non_tap_targets']}=={'mypaint-enum-library-only','painter-filter-normalize-shadow'}

def validate_erratum(receipt,rows):
 done=[x for r in rows for x in taps(r.get('stdout'))[0]];skipped=[x for r in rows for x in taps(r.get('stdout'))[1]]
 assert len(skipped)==5 and receipt['historical_all_ok_lines_count']==len(done)+5
 assert receipt['corrected_executed_non_skip_tap']==len(done)
 assert len(receipt['independent_coverage'])==5
 for trace in receipt['independent_coverage']:
  assert any(trace['skipped_selector'] in x for x in skipped)
  assert trace['independent_completed_runs']
  for hit in trace['independent_completed_runs']:
   r=next(r for r in rows if r['name']==hit['target']);assert r['result']=='OK' and hit['result']=='OK'
   assert hit['line'] in taps(r['stdout'])[0] and hit['line'].split(' ',2)[2]==trace['skipped_selector']

def verify(root):
 root=pathlib.Path(root);assert hashlib.sha256((root/'evidence.tar.gz').read_bytes()).hexdigest()==ARCHIVE
 with tarfile.open(root/'evidence.tar.gz')as t:
  read=lambda n:t.extractfile('exact-'+COMMIT+'/'+n).read();data=lambda n:json.loads(read(n))
  assert read('source-commit.txt').decode().strip()==COMMIT
  pins=data('artifacts-sha256.json');assert len(pins)==59
  for n,h in pins.items():assert hashlib.sha256(read(n)).hexdigest()==h,n
  validate(data('results.json'),data('aggregate-summary.json'),data('extra-ui-results.json'),data('graph-ui-result.json'))
  assert data('final-source-seal.json')=={'checked_files':19601,'changed':[]}
  assert data('postrun-external-data-seal.json')=={'checked_files':1714,'changed':[]}
  assert len(data('native-executable-sha256.json'))==46
  assert data('postrun-held-exclusions.json')=={'held_scopes':7,'providers':11,'all_unchanged':True,'untracked_size_domain_candidate_absent':True}
  transition=data('held-55bf-to-current-comparison.json');assert transition['ten_other_provider_hashes_identical'] and transition['options_exact_single_default_change']
  dims=data('image-dimensions.json');assert len(dims)==31
  for n,d in dims.items():
   expected=[1040,1800]if n.startswith(('physical-screenshots/','geometry-tool-screenshots/'))else [1080,1920]
   b=read(n);assert d==expected and b[:8]==b'\x89PNG\r\n\x1a\n' and list(struct.unpack('>II',b[16:24]))==expected
  scope=data('oncanvas-scope.json');assert scope['portrait_optin']=='GIMP_TEST_REQUIRE_PORTRAIT=1' and scope['not_blanket_every_backend_case_rendered'] and not scope['production_default_changed']
  gates=data('source-gates-summary.json');assert gates['selected_materialized_input_count']==17089 and gates['full_native_tracked_source_count']==19601 and gates['external_asset_count']==1714
  b=read('source-gates.tar.gz');assert hashlib.sha256(b).hexdigest()==gates['source_gates_archive_sha256']
  with tarfile.open(fileobj=io.BytesIO(b),mode='r:gz')as gt:
   gd=lambda n:json.load(gt.extractfile(n))
   assert gd('source-after.json')=={'checked':17089,'changed':[]}
   assert gd('source-manifest.json')['commit']==COMMIT and gd('adapter.json')['status']=='PASS' and gd('persistence.json')['result']=='PASS'
   assert b'CalledProcessError' in gt.extractfile('persistence-missing-object-store.log').read()
  for receipt,folder in zip(data('historical-tap-count-correction.json')['receipts'],['exact-aggregate-503','exact-aggregate-55bf']):
   with tarfile.open(root.parent/folder/'evidence.tar.gz')as previous:
    raw=previous.extractfile('exact-'+receipt['source_commit']+'/results.json').read()
    assert hashlib.sha256(raw).hexdigest()==receipt['original_results_sha256'];validate_erratum(receipt,json.loads(raw))
 print('PASS: exact0a77547 targets;515 non-SKIP TAP,6 skips,2 non-TAP targets;90 selected extra executions; historical count erratum validated')
if __name__=='__main__':verify(pathlib.Path(__file__).resolve().parent)

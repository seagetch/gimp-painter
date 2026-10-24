#!/usr/bin/env python3
"""Read-only admission of private, test-only oncanvas replay; no rebuild."""
import hashlib,json,pathlib,re,struct,tarfile
COMMIT='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94'
ARCHIVE='55e60c0372dd17aaf367f771677b3a71c35e5401da2c47bd106c08a0a4ae5798'
EXPECTED={'painter-smudge-ui':26,'painter-save-continuation':13,'painter-mypaint-tool':27,'paint-geometry-tool':8}
def validate(rows,summary,seal):
 assert {r['target']:r['ok_tap_cases'] for r in rows}==EXPECTED and len(rows)==4
 for r in rows:
  assert r['returncode']==0
  assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==EXPECTED[r['target']]
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in r['stdout']
 assert summary['production_commit']==COMMIT and summary['cases']==74
 assert summary['production_changes']==0 and summary['private_test_overlays']==4
 assert summary['original_assertions_retained'] and summary['actual_app_config_and_associated_shell_visible_asserted']
 assert not summary['new_sanitizer_claim'] and not summary['whole_44_oncanvas_claim']
 assert seal['production_commit']==COMMIT and seal['committed_sources']==19165 and seal['unchanged'] and seal['cases']==74
 assert seal['test_only_overlay'] and seal['linked_inputs']>1000

def verify(root):
 root=pathlib.Path(root);assert hashlib.sha256((root/'evidence.tar.gz').read_bytes()).hexdigest()==ARCHIVE
 with tarfile.open(root/'evidence.tar.gz') as t:
  read=lambda n:t.extractfile('oncanvas-supplement/'+n).read()
  data=lambda n:json.loads(read(n))
  pins=data('artifacts-sha256.json');assert len(pins)==42
  for n,h in pins.items():assert hashlib.sha256(read(n)).hexdigest()==h,n
  validate(data('results.json'),data('summary.json'),data('final-seal.json'))
  owned=data('source-pins.json');assert owned['base_commit']==COMMIT and owned['test_only'] and len(owned['files'])==4
  for n,p in owned['files'].items():
   old=read('base/'+n);new=read(n)
   assert hashlib.sha256(old).hexdigest()==p['base_sha256'] and hashlib.sha256(new).hexdigest()==p['candidate_sha256']
   lines=iter(new.splitlines());assert all(any(x==line for x in lines)for line in old.splitlines()),n
  dims=data('image-dimensions.json');assert len(dims)==5
  for n,d in dims.items():
   expected=[1040,1800] if n.startswith('geometry-') else [1080,1920]
   b=read(n);assert d==expected and b[:8]==b'\x89PNG\r\n\x1a\n' and list(struct.unpack('>II',b[16:24]))==expected
  audit=data('ui-enable-matrix.json');assert audit['source_commit']==COMMIT and len(audit['rows'])==44
  initial=data('setup-missing-menu-results.json');assert initial[0]['returncode']!=0 and initial[0]['ok_tap_cases']==0 and 'failed to read' in initial[0]['stdout']
 print('PASS: private exact55bf production + four fixture overlays; 74 cases, five portrait captures; no blanket44 or sanitizer claim')
if __name__=='__main__':verify(pathlib.Path(__file__).resolve().parent)
